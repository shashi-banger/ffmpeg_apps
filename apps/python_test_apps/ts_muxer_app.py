
import os
import time
import sys
import ffmpeg_lib as ffmpeg
import array as arr
from collections import namedtuple

MAX_VIDEO_FRAME_SIZE = 1000000
MAX_AUDIO_FRAME_SIZE = 64000
frame_header_size = 32
es_info = namedtuple('es_info', "type es_file")
frame_header = namedtuple('frame_header', "size pts dts utc flags")


def cmdline_parse():
    options = {}
    options['out_ts_file'] = sys.argv[1]
    options['vid_codec_type'] = sys.argv[2]
    options['vid_es_file'] = sys.argv[3]

    options['audio_es_files'] = []
    for codec, es_file in zip(sys.argv[4::2], sys.argv[5::2]):
        if codec == "mp2" or codec == "aac" or codec == "ac3":
            es = es_info(codec, es_file)
            options['audio_es_files'].append(es)

    return options

def read_frame_hdr(es_fd):
    b = es_fd.read(4)
    if len(b) == 0:
        return -1, None
    size = int.from_bytes(b, byteorder='little', signed=True)

    b = es_fd.read(8)
    if len(b) == 0:
        return -1, None
    pts = int.from_bytes(b, byteorder='little', signed=True)

    b = es_fd.read(8)
    if len(b) == 0:
        return -1, None
    dts = int.from_bytes(b, byteorder='little', signed=True)

    b = es_fd.read(8)
    if len(b) == 0:
        return -1, None
    utc = int.from_bytes(b, byteorder='little', signed=True)

    b = es_fd.read(4)
    if len(b) == 0:
        return -1, None
    flags = int.from_bytes(b, byteorder='little', signed=True)

    return 0, frame_header(size, pts, dts, utc, flags)


def write_muxed_output(mux, o_buf, mux_out_fd):
    bytes_wr = 0
    while(1):
        ret = ffmpeg.read_muxed_data(mux, o_buf, 188*7)
        if ret == 0:
            mux_out_fd.write(o_buf)
            bytes_wr += 188*7
        else:
            break
    return bytes_wr

def muxer_process(mux, video_es_fd, num_aud,
                  audio_es_fd, mux_out_fd):

    o_buf = bytearray(188*7)
    
    while(1):
        write_muxed_output(mux, o_buf, mux_out_fd)

        ret, video_frame_hdr = read_frame_hdr(video_es_fd)
        if ret == -1:
            break

        print(f"PY_DEBUG: vid, {video_frame_hdr.size}")

        if ffmpeg.get_video_write_avail_size(mux) > video_frame_hdr.size:
            vid_frame = video_es_fd.read(video_frame_hdr.size)
            print("Calling write_video_frame")
            ffmpeg.write_video_frame(mux, vid_frame, video_frame_hdr.size, 
                                     video_frame_hdr.pts, video_frame_hdr.dts)
            #print("Finish write_video_frame")
        else:
            video_es_fd.seek(-frame_header_size, os.SEEK_CUR)
            time.sleep(0.040)

        for i in range(num_aud):
            ret, audio_frame_hdr = read_frame_hdr(audio_es_fd[i])
            if ret == -1:
                break

            print(f"PY_DEBUG: aud, {audio_frame_hdr.size}")

            if (ffmpeg.get_audio_write_avail_size(mux, i) > 
                    audio_frame_hdr.size)  and (
                    audio_frame_hdr.pts <= video_frame_hdr.pts):

                audio_frame = audio_es_fd[i].read(audio_frame_hdr.size)
                ffmpeg.write_audio_frame(mux, audio_frame, audio_frame_hdr.size, 
                                         audio_frame_hdr.pts,
                                         audio_frame_hdr.dts, i)
            else:
                audio_es_fd[i].seek(-frame_header_size, os.SEEK_CUR)
                break

    for i in range(10):
        bytes_wr = write_muxed_output(mux, o_buf, mux_out_fd)
        if bytes_wr == 0:
            time.sleep(0.050)

    
    
def main():

    options = cmdline_parse()

    #a1 = ffmpeg.new_intArray(32)
    #audio_pids = arr.array('i', [0] * 32)
    audio_pids = [0] * 32
    audio_codec = [0] * 32
    params = ffmpeg.ts_muxer_params_t()

    params.vid_pid   = 2064
    params.vid_codec = ffmpeg.AV_CODEC_VID_H264
    params.frame_rate = 29.97

    params.num_aud_tracks = len(options['audio_es_files'])
    audio_es_fd = []
    audio_pid_val = 2068
    for i in range(params.num_aud_tracks ):
        audio_pids[i] = audio_pid_val
        audio_es_fd.append(open(options['audio_es_files'][i].es_file, 'rb'))
        audio_pid_val += 1
        if options['audio_es_files'][i].type == "mp2":
            audio_codec[i] = ffmpeg.AV_CODEC_AUD_MP2
        elif options['audio_es_files]'][i].type == "aac":
            audio_codec[i] = ffmpeg.AV_CODEC_AUD_AAC

    params.aud_codec = audio_codec
    params.aud_pid = audio_pids

    mux = ffmpeg.create_ts_muxer(params)

    print(mux)
 
    video_es_fd = open(options['vid_es_file'], "rb")
    
    mux_out_fd = open(options['out_ts_file'], 'wb')

    muxer_process(mux, video_es_fd, params.num_aud_tracks,
                  audio_es_fd, mux_out_fd)

    


if __name__ == "__main__":
    main()
    
