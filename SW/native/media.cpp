#include "media.hpp"
#include "media_pixels.hpp"
#include <cmath>
#include <filesystem>
#include <stdexcept>
extern "C" {
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/channel_layout.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
namespace sw {
namespace {
void check(int result,const char* operation) {
    if(result>=0)return;
    char text[AV_ERROR_MAX_STRING_SIZE]{};av_strerror(result,text,sizeof(text));
    throw std::runtime_error(std::string(operation)+": "+text);
}
struct Reader {
    AVFormatContext* demux=nullptr;
    AVCodecContext* codec=nullptr;
    AVPacket* packet=av_packet_alloc();
    AVFrame* frame=av_frame_alloc();
    AVFrame* host=av_frame_alloc();
    bool hardware=false;
    int index=-1;bool drained=false;
    Reader(const std::string& path,AVMediaType type,int ordinal,std::atomic<bool>& quit) {
        try {
            if(!packet||!frame||!host)throw std::bad_alloc();
            demux=avformat_alloc_context();if(!demux)throw std::bad_alloc();
            demux->interrupt_callback={[](void* p){return static_cast<std::atomic<bool>*>(p)->load()?1:0;},&quit};
            // Local files only. Do not let a media filename enable network protocols.
            AVDictionary* options=nullptr;av_dict_set(&options,"protocol_whitelist","file",0);
            int r=avformat_open_input(&demux,path.c_str(),nullptr,&options);av_dict_free(&options);check(r,"Open media");
            check(avformat_find_stream_info(demux,nullptr),"Read media information");
            for(unsigned i=0;i<demux->nb_streams;++i)if(demux->streams[i]->codecpar->codec_type==type&&ordinal--==0){index=int(i);break;}
            if(index<0)return;
            for(unsigned i=0;i<demux->nb_streams;++i)if(int(i)!=index)demux->streams[i]->discard=AVDISCARD_ALL;
            auto* decoder=avcodec_find_decoder(stream()->codecpar->codec_id);
            if(!decoder)throw std::runtime_error("File codec is unavailable in FFmpeg.");
            codec=avcodec_alloc_context3(decoder);if(!codec)throw std::bad_alloc();
            check(avcodec_parameters_to_context(codec,stream()->codecpar),"Set decoder parameters");
            codec->pkt_timebase=stream()->time_base;codec->thread_count=4;
            if(type==AVMEDIA_TYPE_VIDEO&&codec->width>=1920)for(int i=0;;++i) {
                const auto* config=avcodec_get_hw_config(decoder,i);if(!config)break;
                if(config->device_type==AV_HWDEVICE_TYPE_CUDA&&(config->methods&AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)) {
                    if(av_hwdevice_ctx_create(&codec->hw_device_ctx,AV_HWDEVICE_TYPE_CUDA,nullptr,nullptr,0)>=0)
                        codec->get_format=[](AVCodecContext* context,const AVPixelFormat* formats) {
                            for(auto* p=formats;*p!=AV_PIX_FMT_NONE;++p)if(*p==AV_PIX_FMT_CUDA)return *p;
                            return avcodec_default_get_format(context,formats);
                        };
                    break;
                }
            }
            check(avcodec_open2(codec,decoder,nullptr),"Open decoder");
        } catch(...) {release();throw;}
    }
    ~Reader(){release();}
    void release(){av_frame_free(&host);av_frame_free(&frame);av_packet_free(&packet);avcodec_free_context(&codec);avformat_close_input(&demux);}
    AVStream* stream()const{return demux->streams[index];}
    AVFrame* output()const{return hardware?host:frame;}
    bool next() {
        if(index<0)return false;
        for(;;) {
            av_frame_unref(frame);int r=avcodec_receive_frame(codec,frame);
            if(r==0) {
                hardware=frame->format==AV_PIX_FMT_CUDA;
                if(hardware){check(av_hwframe_transfer_data(host,frame,0),"Download decoded video");check(av_frame_copy_props(host,frame),"Copy video metadata");}
                return true;
            }
            if(r==AVERROR_EOF)return false;
            if(r!=AVERROR(EAGAIN))check(r,"Decode media");
            if(drained)return false;
            do {av_packet_unref(packet);r=av_read_frame(demux,packet);}while(r>=0&&packet->stream_index!=index);
            if(r<0) {if(r!=AVERROR_EOF)check(r,"Read packet");drained=true;check(avcodec_send_packet(codec,nullptr),"Drain decoder");}
            else check(avcodec_send_packet(codec,packet),"Submit packet");
        }
    }
    void seek(double seconds) {
        if(index<0)return;
        const auto origin=stream()->start_time==AV_NOPTS_VALUE?0:stream()->start_time;
        const auto target=origin+int64_t(seconds/av_q2d(stream()->time_base));
        check(av_seek_frame(demux,index,target,AVSEEK_FLAG_BACKWARD),"Seek media");
        avcodec_flush_buffers(codec);av_packet_unref(packet);drained=false;
    }
};
struct AudioReader {
    struct Block{int64_t start=0;std::vector<int32_t> data;};
    Reader reader;SwrContext* resampler=nullptr;
    std::deque<Block> blocks;
    int channels=0;int64_t origin=0,end=0;bool eof=false;
    AudioReader(const std::string& path,int ordinal,int64_t videoOrigin,std::atomic<bool>& quit):reader(path,AVMEDIA_TYPE_AUDIO,ordinal,quit),origin(videoOrigin) {
        if(reader.index<0)return;
        try {
            auto* c=reader.codec;channels=c->ch_layout.nb_channels;
            if(channels<1||channels>64)throw std::runtime_error("Unsupported file audio channel layout.");
            check(swr_alloc_set_opts2(&resampler,&c->ch_layout,AV_SAMPLE_FMT_S32,48000,&c->ch_layout,c->sample_fmt,c->sample_rate,0,nullptr),"Create audio converter");
            check(swr_init(resampler),"Initialize audio converter");
        }catch(...){swr_free(&resampler);throw;}
    }
    ~AudioReader(){swr_free(&resampler);}
    void seek(double seconds){
        if(reader.index>=0){const auto start=reader.stream()->start_time==AV_NOPTS_VALUE?0:reader.stream()->start_time;reader.seek(std::max(0.,seconds+double(origin)/48000-start*av_q2d(reader.stream()->time_base)));}
        blocks.clear();end=0;eof=false;if(resampler){swr_close(resampler);check(swr_init(resampler),"Reset audio converter");}
    }
    void next() {
        const bool valid=reader.next();auto* f=reader.frame;
        int64_t start=end;
        if(valid&&f->best_effort_timestamp!=AV_NOPTS_VALUE)
            start=av_rescale_q(f->best_effort_timestamp,reader.stream()->time_base,AVRational{1,48000})-origin-swr_get_delay(resampler,48000);
        const int capacity=swr_get_out_samples(resampler,valid?f->nb_samples:0);
        Block block;block.start=start;block.data.resize(size_t(std::max(capacity,1))*channels);
        uint8_t* destination=reinterpret_cast<uint8_t*>(block.data.data());
        int n=swr_convert(resampler,&destination,std::max(capacity,1),valid?const_cast<const uint8_t**>(f->extended_data):nullptr,valid?f->nb_samples:0);
        check(n,"Convert audio");block.data.resize(size_t(n)*channels);
        if(n){end=start+n;blocks.push_back(std::move(block));}
        if(!valid&&!n)eof=true;
    }
    void copy(int64_t first,int count,int sourceChannel,std::vector<int32_t>& target,int targetChannel) {
        if(reader.index<0)return;
        const auto last=first+count;
        while(!eof&&end<last) {
            next();
            while(!blocks.empty()&&blocks.front().start+int64_t(blocks.front().data.size()/channels)<=first)blocks.pop_front();
            if(blocks.size()>4096)throw std::runtime_error("Audio timestamps exceed bounded decode buffer.");
        }
        for(const auto& b:blocks) {
            const auto begin=std::max(first,b.start),finish=std::min(last,b.start+int64_t(b.data.size()/channels));
            for(auto t=begin;t<finish;++t)target[size_t(t-first)*2+targetChannel]=b.data[size_t(t-b.start)*channels+sourceChannel];
        }
    }
};
}
MediaPlayer::~MediaPlayer(){close();}
void MediaPlayer::open(std::string path,Format format,bool loop) {
    close();
    if(!std::filesystem::is_regular_file(std::filesystem::u8path(path)))throw std::runtime_error("Select an existing local media file.");
    {std::lock_guard lock(mutex_);quit_=false;status_={};status_.active=true;status_.loop=loop;seek_=0;++generation_;}
    worker_=std::thread([this,path=std::move(path),format]{decode(path,format);});
}
void MediaPlayer::close() {
    quit_=true;wake_.notify_all();if(worker_.joinable())worker_.join();
    std::lock_guard lock(mutex_);queue_.clear();held_.reset();pcm_.clear();status_.active=status_.ready=status_.playing=false;
}
void MediaPlayer::play(){std::lock_guard lock(mutex_);if(!status_.active||!status_.error.empty())return;if(status_.eof){seek_=0;++generation_;queue_.clear();held_.reset();pcm_.clear();status_.ready=false;status_.eof=false;}status_.playing=true;wake_.notify_all();}
void MediaPlayer::pause(){std::lock_guard lock(mutex_);status_.playing=false;pcm_.clear();}
void MediaPlayer::cue(){std::lock_guard lock(mutex_);status_.playing=false;seek_=0;++generation_;queue_.clear();held_.reset();pcm_.clear();status_.eof=status_.ready=false;status_.position=0;wake_.notify_all();}
void MediaPlayer::seek(double seconds){std::lock_guard lock(mutex_);if(!std::isfinite(seconds))return;seek_=std::clamp(seconds,0.,std::max(0.,status_.duration-.04));++generation_;queue_.clear();held_.reset();pcm_.clear();status_.eof=status_.ready=false;wake_.notify_all();}
void MediaPlayer::setLoop(bool b){std::lock_guard lock(mutex_);status_.loop=b;wake_.notify_all();}
MediaStatus MediaPlayer::status()const{std::lock_guard lock(mutex_);auto s=status_;s.buffered=queue_.size();return s;}
MediaSample MediaPlayer::pull(uint32_t samples) {
    std::lock_guard lock(mutex_);MediaSample result;result.audio.assign(size_t(samples)*2,0);
    if(!queue_.empty()) {
        if(status_.playing) {
            if(held_&&queue_.front()->sequence<=held_->sequence)pcm_.clear();
            held_=queue_.front();queue_.pop_front();++status_.frames;
            // Keep fractional sample phase across frames. A seek can require one
            // initial padding pair, but must not repeat/drop a pair every frame.
            pcm_.insert(pcm_.end(),held_->audio.begin(),held_->audio.end());
            for(size_t i=0;i<samples;++i)for(size_t c=0;c<2;++c) {
                if(!pcm_.empty()){result.audio[i*2+c]=pcm_.front();pcm_.pop_front();}
                else if(i)result.audio[i*2+c]=result.audio[(i-1)*2+c];
            }
            wake_.notify_all();
        }else if(!held_)held_=queue_.front();
        if(held_)status_.position=double(held_->sequence)*held_->format.frameDuration/held_->format.timeScale;
    }else if(status_.playing&&!status_.eof)++status_.underruns;
    result.video=held_;return result;
}
void MediaPlayer::decode(std::string path,Format format) {
    SwsContext* scaler=nullptr;
    try {
        Reader video(path,AVMEDIA_TYPE_VIDEO,0,quit_);
        if(video.index<0)throw std::runtime_error("The selected file has no video stream.");
        auto* stream=video.stream();auto* parameters=stream->codecpar;
        const auto rate=av_guess_frame_rate(video.demux,stream,nullptr);
        if(parameters->width!=format.width||parameters->height!=format.height||std::abs(av_q2d(rate)-double(format.timeScale)/format.frameDuration)>.002)
            throw std::runtime_error("File must match session: "+format.label()+". Convert other sizes/rates before loading.");
        const int64_t start=stream->start_time==AV_NOPTS_VALUE?0:stream->start_time;
        const int64_t audioOrigin=av_rescale_q(start,stream->time_base,AVRational{1,48000});
        AudioReader left(path,0,audioOrigin,quit_);
        std::unique_ptr<AudioReader> right;
        if(left.channels==1)right=std::make_unique<AudioReader>(path,1,audioOrigin,quit_);
        {std::lock_guard lock(mutex_);status_.duration=stream->duration!=AV_NOPTS_VALUE?stream->duration*av_q2d(stream->time_base):(video.demux->duration==AV_NOPTS_VALUE?0.:double(video.demux->duration)/AV_TIME_BASE);}
        uint64_t generation=0;int64_t wanted=0,fallback=0,lastAccepted=-1;bool end=false;FramePool pool{16};
        for(;;) {
            double seekTarget=0;bool seekRequested=false;
            {std::unique_lock lock(mutex_);
             wake_.wait(lock,[&]{return quit_||generation!=generation_||(!end&&queue_.size()<8)||(end&&queue_.empty()&&status_.playing);});
             if(quit_)break;
             if(end&&queue_.empty()&&status_.playing&&generation==generation_) {
                 if(status_.loop){seek_=0;++generation_;}else{status_.playing=false;status_.eof=true;continue;}
             }
             if(generation!=generation_){seekTarget=seek_;generation=generation_;seekRequested=true;}
            }
            if(seekRequested) {
                if(seekTarget>0||end||fallback>0){video.seek(seekTarget);left.seek(seekTarget);if(right)right->seek(seekTarget);}
                wanted=int64_t(seekTarget*format.timeScale/format.frameDuration);fallback=wanted;lastAccepted=-1;end=false;
            }
            const auto begin=std::chrono::steady_clock::now();
            if(!video.next()){end=true;continue;}
            const auto decodedAt=std::chrono::steady_clock::now();
            auto* decoded=video.output();
            if(decoded->width!=format.width||decoded->height!=format.height)throw std::runtime_error("File resolution changed during playback.");
            const bool interlaced=(decoded->flags&AV_FRAME_FLAG_INTERLACED)!=0;
            if(interlaced!=format.interlaced||(interlaced&&!(decoded->flags&AV_FRAME_FLAG_TOP_FIELD_FIRST)))
                throw std::runtime_error("File scan mode must match session (HD: interlaced, top field first; UHD: progressive).");
            int64_t index=decoded->best_effort_timestamp==AV_NOPTS_VALUE?fallback:av_rescale_q(decoded->best_effort_timestamp-start,stream->time_base,AVRational{int(format.frameDuration),int(format.timeScale)});
            fallback=index+1;if(index<wanted)continue;
            if(lastAccepted>=0&&index!=lastAccepted+1)throw std::runtime_error("Variable-rate or discontinuous video timestamps are unsupported. Convert to the session's constant frame rate.");
            lastAccepted=index;
            auto frame=pool.acquire(format);if(!frame)throw std::runtime_error("Player frame pool exhausted.");frame->sequence=uint64_t(std::max<int64_t>(0,index));
            scaler=sws_getCachedContext(scaler,decoded->width,decoded->height,AVPixelFormat(decoded->format),format.width,format.height,AV_PIX_FMT_UYVY422,SWS_BILINEAR,nullptr,nullptr,nullptr);
            if(!scaler)throw std::runtime_error("Unable to convert video to switcher UYVY format.");
            if(decoded->colorspace==AVCOL_SPC_BT2020_NCL||decoded->color_trc==AVCOL_TRC_SMPTE2084||decoded->color_trc==AVCOL_TRC_ARIB_STD_B67)
                throw std::runtime_error("HDR/BT.2020 media requires SDR Rec.709 conversion before loading.");
            const int* coefficients=sws_getCoefficients(SWS_CS_ITU709);
            check(sws_setColorspaceDetails(scaler,coefficients,decoded->color_range==AVCOL_RANGE_JPEG,coefficients,0,0,1<<16,1<<16),"Set Rec.709 video range");
            uint8_t* pixels[4]={frame->data.data(),nullptr,nullptr,nullptr};int strides[4]={frame->stride,0,0,0};
            const auto pixelFormat=AVPixelFormat(decoded->format);
            const bool planar=pixelFormat==AV_PIX_FMT_YUV420P||pixelFormat==AV_PIX_FMT_YUV422P||pixelFormat==AV_PIX_FMT_YUV420P10LE||pixelFormat==AV_PIX_FMT_YUV422P10LE;
            const bool semi=pixelFormat==AV_PIX_FMT_NV12||pixelFormat==AV_PIX_FMT_NV16||pixelFormat==AV_PIX_FMT_P010LE||pixelFormat==AV_PIX_FMT_P210LE;
            if((planar||semi)&&decoded->color_range!=AVCOL_RANGE_JPEG) {
                const bool half=pixelFormat==AV_PIX_FMT_YUV420P||pixelFormat==AV_PIX_FMT_YUV420P10LE||pixelFormat==AV_PIX_FMT_NV12||pixelFormat==AV_PIX_FMT_P010LE;
                const bool wide=pixelFormat==AV_PIX_FMT_YUV420P10LE||pixelFormat==AV_PIX_FMT_YUV422P10LE||pixelFormat==AV_PIX_FMT_P010LE||pixelFormat==AV_PIX_FMT_P210LE;
                const uint8_t* planes[3]={decoded->data[0],decoded->data[1],decoded->data[2]};
                packFileYuv(planes,decoded->linesize,pixels[0],strides[0],format.width,format.height,planar,half,wide,planar?2:8,interlaced);
            }else check(sws_scale(scaler,decoded->data,decoded->linesize,0,decoded->height,pixels,strides),"Convert video");
            const auto convertedAt=std::chrono::steady_clock::now();
            const auto n=audioSamples(frame->sequence,format);frame->audio.assign(size_t(n)*2,0);
            const auto sampleStart=int64_t(audioSampleTime(frame->sequence,format));
            left.copy(sampleStart,int(n),0,frame->audio,0);
            if(left.channels>1)left.copy(sampleStart,int(n),1,frame->audio,1);
            else if(right&&right->channels>0)right->copy(sampleStart,int(n),0,frame->audio,1);
            else left.copy(sampleStart,int(n),0,frame->audio,1);
            frame->captured=std::chrono::steady_clock::now();
            frame->persistent=true;
            {std::lock_guard lock(mutex_);if(generation!=generation_)continue;queue_.push_back(frame);status_.ready=true;status_.decodeMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
             status_.hardware=video.hardware;status_.videoMs=std::chrono::duration<double,std::milli>(decodedAt-begin).count();status_.convertMs=std::chrono::duration<double,std::milli>(convertedAt-decodedAt).count();status_.audioMs=std::chrono::duration<double,std::milli>(frame->captured-convertedAt).count();}
        }
    }catch(const std::exception& e){std::lock_guard lock(mutex_);if(!quit_){status_.error=e.what();status_.playing=false;}}
    sws_freeContext(scaler);
}
}
