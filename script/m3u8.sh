ffmpeg -y  -i 48.mp4  -codec copy -map 0  -f segment  -segment_time 10  -segment_format mpegts  -segment_list "./vod/prog_index.m3u8"  -segment_list_type m3u8  "./vod/fileSequence%d.ts"
