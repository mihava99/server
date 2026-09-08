/*
 * This file is part of CasparCG (www.casparcg.com).
 *
 * CasparCG is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * CasparCG is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with CasparCG. If not, see <http://www.gnu.org/licenses/>.
 */
#include "omt_consumer.h"

#include "../util/omt_util.h"

#include <common/diagnostics/graph.h>
#include <common/env.h>
#include <common/executor.h>
#include <common/future.h>
#include <common/log.h>
#include <common/os/thread.h>
#include <common/param.h>
#include <common/timer.h>
#include <common/utf.h>

#include <core/consumer/channel_info.h>
#include <core/consumer/frame_consumer.h>
#include <core/frame/frame.h>
#include <core/video_format.h>

#include <boost/algorithm/string.hpp>
#include <boost/property_tree/ptree.hpp>

extern "C" {
#include <libavutil/channel_layout.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
}

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>
#include <vector>

namespace caspar { namespace omt {

namespace {

OMTQuality parse_quality(const std::wstring& value)
{
    if (boost::iequals(value, L"low"))
        return OMTQuality_Low;
    if (boost::iequals(value, L"medium"))
        return OMTQuality_Medium;
    if (boost::iequals(value, L"high"))
        return OMTQuality_High;
    return OMTQuality_Default;
}

// CasparCG composites internally with straight-multiplied (i.e. premultiplied) alpha - see e.g.
// the bluefish consumer's keyer setup. Some receivers (e.g. OBS's OMT support, as of this writing)
// mishandle alpha entirely - ignoring OMTVideoFlags_PreMultiplied, and sometimes still getting
// straight alpha wrong too - so besides converting to straight, sending no alpha channel at all
// is offered as a compatibility escape hatch for those.
enum class alpha_mode
{
    premultiplied,
    straight,
    none
};

alpha_mode parse_alpha_mode(const std::wstring& value)
{
    if (boost::iequals(value, L"straight"))
        return alpha_mode::straight;
    if (boost::iequals(value, L"none"))
        return alpha_mode::none;
    return alpha_mode::premultiplied;
}

const wchar_t* alpha_mode_name(alpha_mode mode)
{
    switch (mode) {
        case alpha_mode::straight:
            return L"straight";
        case alpha_mode::none:
            return L"none";
        default:
            return L"premultiplied";
    }
}

// 16.16 fixed-point reciprocal of alpha/255, used to unpremultiply BGRA pixels: straight = premultiplied * 255 / alpha.
struct unpremultiply_reciprocal_table
{
    std::array<uint32_t, 256> value;

    unpremultiply_reciprocal_table()
    {
        value[0] = 0; // unused - alpha 0 pixels are left as-is below.
        for (int a = 1; a < 256; ++a)
            value[static_cast<size_t>(a)] = (255u * 65536u + static_cast<uint32_t>(a) / 2) / static_cast<uint32_t>(a);
    }
};

void unpremultiply_bgra(uint8_t* data, size_t byte_count)
{
    static const unpremultiply_reciprocal_table table;

    for (size_t offset = 0; offset + 4 <= byte_count; offset += 4) {
        uint8_t* pixel = data + offset;
        uint8_t  alpha = pixel[3];

        if (alpha == 0 || alpha == 255)
            continue; // Fully transparent or fully opaque - premultiplied and straight are identical.

        uint32_t reciprocal = table.value[alpha];
        pixel[0]            = static_cast<uint8_t>(std::min<uint32_t>(255, (pixel[0] * reciprocal + 32768) >> 16));
        pixel[1]            = static_cast<uint8_t>(std::min<uint32_t>(255, (pixel[1] * reciprocal + 32768) >> 16));
        pixel[2]            = static_cast<uint8_t>(std::min<uint32_t>(255, (pixel[2] * reciprocal + 32768) >> 16));
    }
}

} // namespace

struct omt_consumer : public core::frame_consumer
{
    static std::atomic<int> instances_;
    const int                instance_no_;
    const std::wstring       name_;
    const std::string        name_utf8_;
    const OMTQuality         quality_;
    const alpha_mode         alpha_mode_;

    core::video_format_desc format_desc_;
    int                     channel_index_ = 0;

    omt_lib*             lib_  = nullptr;
    omt_send_t*          send_ = nullptr;
    std::vector<uint8_t> straight_alpha_buffer_;

    struct swr_deleter
    {
        void operator()(SwrContext* p) { swr_free(&p); }
    };
    std::unique_ptr<SwrContext, swr_deleter> swr_;

    std::mutex                    buffer_mutex_;
    std::condition_variable       buffer_cond_;
    std::queue<core::const_frame> buffer_;
    std::atomic<bool>             running_{false};
    std::thread                   send_thread_;

    spl::shared_ptr<diagnostics::graph> graph_;
    caspar::timer                       tick_timer_;
    caspar::timer                       frame_timer_;

  public:
    omt_consumer(std::wstring name, OMTQuality quality, alpha_mode alpha)
        : instance_no_(instances_++)
        , name_(!name.empty() ? name : default_omt_name())
        , name_utf8_(u8(!name.empty() ? name : default_omt_name()))
        , quality_(quality)
        , alpha_mode_(alpha)
    {
        lib_ = omt::load_library();

        graph_ = spl::make_shared<diagnostics::graph>();
        graph_->set_text(print());
        graph_->set_color("frame-time", diagnostics::color(0.5f, 1.0f, 0.2f));
        graph_->set_color("tick-time", diagnostics::color(0.0f, 0.6f, 0.9f));
        graph_->set_color("buffered-frames", diagnostics::color(0.5f, 0.0f, 0.2f));
        diagnostics::register_graph(graph_);
    }

    ~omt_consumer()
    {
        running_ = false;
        buffer_cond_.notify_all();
        if (send_thread_.joinable())
            send_thread_.join();

        if (send_)
            lib_->send_destroy(send_);
    }

    std::wstring default_omt_name() const
    {
        return L"CasparCG" + (instance_no_ ? L" " + std::to_wstring(instance_no_) : L"");
    }

    // frame_consumer

    void initialize(const core::video_format_desc& format_desc,
                    const core::channel_info&      channel_info,
                    int                            port_index) override
    {
        format_desc_   = format_desc;
        channel_index_ = channel_info.index;

        send_ = lib_->send_create(name_utf8_.c_str(), quality_);

        AVChannelLayout layout;
        av_channel_layout_default(&layout, format_desc_.audio_channels);

        SwrContext* raw = nullptr;
        swr_alloc_set_opts2(&raw,
                            &layout,
                            AV_SAMPLE_FMT_FLTP,
                            format_desc_.audio_sample_rate,
                            &layout,
                            AV_SAMPLE_FMT_S32,
                            format_desc_.audio_sample_rate,
                            0,
                            nullptr);
        swr_.reset(raw);
        swr_init(raw);

        graph_->set_text(print());

        CASPAR_LOG(info) << print() << L" sending " << alpha_mode_name(alpha_mode_) << L" alpha.";

        running_    = true;
        send_thread_ = std::thread([this] { run(); });
    }

    void run()
    {
        set_thread_name(L"OMT-SEND: " + name_);

        while (running_) {
            core::const_frame frame;
            {
                std::unique_lock<std::mutex> lock(buffer_mutex_);
                buffer_cond_.wait_for(lock, std::chrono::milliseconds(100), [&] { return !buffer_.empty() || !running_; });
                if (!running_)
                    return;
                if (buffer_.empty())
                    continue;
                frame = std::move(buffer_.front());
                buffer_.pop();
                graph_->set_value("buffered-frames", static_cast<double>(buffer_.size() + 0.001) / 8);
            }

            frame_timer_.restart();
            send_video(frame);
            send_audio(frame);
            graph_->set_value("frame-time", frame_timer_.elapsed() * format_desc_.fps * 0.5);
        }
    }

    void send_video(const core::const_frame& frame)
    {
        auto image       = frame.image_data(0);
        auto data_length = image.size();
        if (!send_ || data_length == 0)
            return;

        const uint8_t* data  = image.data();
        OMTVideoFlags  flags = OMTVideoFlags_None;

        switch (alpha_mode_) {
            case alpha_mode::none:
                // Leave the Alpha flag unset - per the OMT spec this tells receivers the 4th byte
                // is padding (BGRX rather than BGRA), which sidesteps alpha handling bugs entirely
                // in receivers that get either convention wrong.
                break;
            case alpha_mode::straight:
                // CasparCG's own buffers are premultiplied; convert a copy to straight alpha for
                // receivers (e.g. OBS) that don't understand OMTVideoFlags_PreMultiplied.
                flags = OMTVideoFlags_Alpha;
                straight_alpha_buffer_.assign(data, data + data_length);
                unpremultiply_bgra(straight_alpha_buffer_.data(), straight_alpha_buffer_.size());
                data = straight_alpha_buffer_.data();
                break;
            case alpha_mode::premultiplied:
            default:
                flags = static_cast<OMTVideoFlags>(OMTVideoFlags_Alpha | OMTVideoFlags_PreMultiplied);
                break;
        }

        OMTMediaFrame video_frame = {};
        video_frame.Type          = OMTFrameType_Video;
        video_frame.Timestamp     = -1; // Let the sender generate accurate timestamps and pace to FrameRateN/D.
        video_frame.Codec         = OMTCodec_BGRA;
        video_frame.Width         = static_cast<int>(format_desc_.width);
        video_frame.Height        = static_cast<int>(format_desc_.height);
        video_frame.Stride        = static_cast<int>(format_desc_.width) * 4;
        video_frame.Flags         = flags;
        video_frame.FrameRateN    = format_desc_.framerate.numerator();
        video_frame.FrameRateD    = format_desc_.framerate.denominator();
        video_frame.AspectRatio   = static_cast<float>(format_desc_.square_width) / static_cast<float>(format_desc_.square_height);
        video_frame.ColorSpace    = format_desc_.height >= 720 ? OMTColorSpace_BT709 : OMTColorSpace_BT601;
        video_frame.Data          = const_cast<uint8_t*>(data);
        video_frame.DataLength    = static_cast<int>(data_length);

        lib_->send(send_, &video_frame);
    }

    void send_audio(const core::const_frame& frame)
    {
        if (!send_ || !swr_)
            return;

        auto audio_in    = frame.audio_data();
        auto channels    = format_desc_.audio_channels;
        int  num_samples = channels > 0 ? static_cast<int>(audio_in.size()) / channels : 0;

        if (num_samples <= 0)
            return;

        std::vector<float>    planar(static_cast<size_t>(channels) * num_samples);
        std::vector<uint8_t*> out_planes(channels);
        for (int ch = 0; ch < channels; ++ch)
            out_planes[ch] = reinterpret_cast<uint8_t*>(planar.data() + static_cast<size_t>(ch) * num_samples);

        const uint8_t* in_ptr = reinterpret_cast<const uint8_t*>(audio_in.data());
        swr_convert(swr_.get(), out_planes.data(), num_samples, &in_ptr, num_samples);

        OMTMediaFrame audio_frame      = {};
        audio_frame.Type               = OMTFrameType_Audio;
        audio_frame.Timestamp          = -1;
        audio_frame.Codec              = OMTCodec_FPA1;
        audio_frame.SampleRate         = format_desc_.audio_sample_rate;
        audio_frame.Channels           = channels;
        audio_frame.SamplesPerChannel  = num_samples;
        audio_frame.Data               = planar.data();
        audio_frame.DataLength         = static_cast<int>(planar.size() * sizeof(float));

        lib_->send(send_, &audio_frame);
    }

    std::future<bool> send(const core::video_field /*field*/, core::const_frame frame) override
    {
        graph_->set_value("tick-time", tick_timer_.elapsed() * format_desc_.fps * 0.5);
        tick_timer_.restart();
        {
            std::lock_guard<std::mutex> lock(buffer_mutex_);
            buffer_.push(std::move(frame));
            if (buffer_.size() > 4)
                buffer_.pop();
        }
        buffer_cond_.notify_all();
        return caspar::make_ready_future(true);
    }

    std::wstring print() const override
    {
        if (channel_index_)
            return L"omt[" + std::to_wstring(channel_index_) + L"|" + name_ + L"]";
        return L"[omt_consumer]";
    }

    std::wstring name() const override { return L"omt"; }

    int index() const override { return 910; }

    bool has_synchronization_clock() const override { return false; }

    core::monitor::state state() const override
    {
        core::monitor::state state;
        state["omt/name"]  = u8(name_);
        state["omt/alpha"] = u8(alpha_mode_name(alpha_mode_));
        return state;
    }
};

std::atomic<int> omt_consumer::instances_(0);

spl::shared_ptr<core::frame_consumer>
create_omt_consumer(const std::vector<std::wstring>&                         params,
                    const core::video_format_repository&                     format_repository,
                    const std::vector<spl::shared_ptr<core::video_channel>>& channels,
                    const core::channel_info&                                channel_info)
{
    if (params.empty() || !boost::iequals(params.at(0), L"OMT"))
        return core::frame_consumer::empty();

    std::wstring name    = get_param(L"NAME", params, L"");
    std::wstring quality = get_param(L"QUALITY", params, L"");
    std::wstring alpha   = get_param(L"ALPHA", params, L"");

    return spl::make_shared<omt_consumer>(name, parse_quality(quality), parse_alpha_mode(alpha));
}

spl::shared_ptr<core::frame_consumer>
create_preconfigured_omt_consumer(const boost::property_tree::wptree&                      element,
                                  const core::video_format_repository&                     format_repository,
                                  const std::vector<spl::shared_ptr<core::video_channel>>& channels,
                                  const core::channel_info&                                channel_info)
{
    auto name    = element.get(L"name", L"");
    auto quality = element.get(L"quality", L"");
    auto alpha   = element.get(L"alpha", L"");

    return spl::make_shared<omt_consumer>(name, parse_quality(quality), parse_alpha_mode(alpha));
}

}} // namespace caspar::omt
