### Cấu trúc lệnh

- Dạng json object phân biệt bởi type:
  + **info**: lấy thông tin input
  + **start**: bật kênh
  + **stop**: tắt kênh
  + **progress**: lấy danh sách trạng thái kênh
  + **dump**: lấy thông tin máy
  + **update**: thay đổi config volume, overlay, drm, header
  + **event**: đẩy event vào id3 str
  + **ads**: hiện quảng cáo logo + chữ chạy
  + **scte35**: chèn luồng và tín hiệu quảng cáo
  + **mixer**: trộn luồng input 
  + **join**: nối video đã record 
  + **apps**: chạy app tích hợp sẵn(nginx, srs)
  + **stream**: phát list video
  + **subtitle**: transcode subtitle
  + **awt**: audio watermark
### 1. Lấy thông tin input

***Request***

 ```json
{
  "type": "info",
  "timeout": 20,
  "detectKeyframe": 1,
  "input": "udp://225.2.100.228:5000"
}
 ```
 
 - ***timeout***: số giây
 - ***detectKeyframe***: [0, 1], bật chế độ đo keyframe

***Response***

```json
{
  "ec": 0,
  "result": {
    "rtmp://localhost/live/test": [
      {
        "type": "data",
        "codec": "none",
        "bitrate": 0
      },
      {
        "type": "audio",
        "codec": "aac",
        "bitrate": 128696,
        "channel": 2,
        "sampleRate": 48000,
        "profile": "LC"
      },
      {
        "type": "video",
        "codec": "h264",
        "bitrate": 2402563,
        "width": 1280,
        "height": 720,
        "fps": "25",
        "interlaced": 0,
        "pixelFormat": "yuv420p",
        "profile": "High",
        "level": "3.1",
        "kf": "0,2000,4000"
      }
    ]
  },
  "dt": 1605
}
```

 - ***kf***: danh sách keyframe

### 2. Bật kênh

```json
{
  "type": "start",
  "name": "vtv3",
  "gpu": -1,
  "input": {
    "main": "udp://234.5.6.7:8888",
    "backup": "udp://234.5.6.7:7777"
  },
  "target": [
    {
      "enable": 0,
      "format": "rtmp",
      "url": "rtmp://localhost/app/stream",
      "select": [
        "128k",
        "360"
      ]
    },
    {
      "enable": 1,
      "format": "hls",
      "url": "/mnt/ramdisk/origin04/source/",
      "select": [
        "128k",
        "360"
      ]
    },
    {
      "enable": 0,
      "format": "dash",
      "url": "/mnt/ramdisk/origin04/360/",
      "select": [
        "128k",
        "360",
        "source"
      ],
      "manifest":{
      },
      "drm:{
      }
    }
  ],
  "profile": [
    {
      "enable": 0,
      "id": "data",
      "type": "data",
      "codec": "copy",
      "streamIndex": 0,
      "sourceIndex": 0,
      "iframe": 50
    },
    {
      "id": "96k",
      "type": "audio",
      "codec": "aac",
      "bitrate": 96000
    },
    {
      "id": "360",
      "type": "video",
      "codec": "h264",
      "width": 640,
      "height": 360,
      "bitrate": 500000,
      "cbr": 1,
      "pixelFormat": "yuv420p",
      "watermark": [
        {
          "enable": 1,
          "x": 10,
          "y": 20,
          "url": "wm.png"
        }
      ]
    },
    {
      "id": "source",
      "type": "video",
      "codec": "copy"
    }
  ],
  "option": {
    "syncCopy": 1,
    "blackout": {
      "input": "blackout.jpg"
    }
  },
  "watermark":
  {
    "enable": 1,
    "x": 10,
    "y": 20,
    "url": "wm.png"
  },
  "thumb": {
    "enable": 1,
    "width": 300,
    "delay": 6,
    "name":"%d.jpg",
    "url": "/mnt/ramdisk/origin05/trial/thumb/"
  }
}

```

  - ***name***: định danh kênh để bật tắt
  - ***gpu***: thứ tự gpu chip, danh sách gpu lấy ở lệnh dump(chỉ hỗ trợ nvidia), -1 là sử dụng cpu
  - ***input***: danh sách input
  - ***profile***: danh sách profile
  - ***target***: danh sách target
  - ***watermark***: watermark chung cho kênh, nếu nhiều dùng array
  - ***thumb***: tạo thumnail cho kênh, nếu nhiều dùng array
  - ***option***: các cài đặt thêm
  - ***input***: có thể là 1 string, mảng string(nhiều input), hoặc 1 object cho kênh có backup
* **profile**
  - ***id***: định danh của profile, nếu ở dạng hex[0x10->0x1ffe] sẽ dùng làm stream_id ở video đầu ra
  - ***streamIndex***: thứ tự của stream gốc trong input, ví dụ input có 2 audio thì có thể set cho audio là 0, 1
  - ***sourceIndex***: thứ tự của input
  - ***type***: [video, audio, data]
  - **audio**:
    + ***sampleRate***: [44100(default), 48000]
    + ***channel***: [1, 2(default), 6(5.1)]
    + ***codec***: [copy, aac(default), ac3, eac3, mp2]
    + ***profile***(chỉ cho aac): [aac_low(default), aac_he, aac_he_v2, aac_main, aac_ld, aac_eld]
    + ***volume***: thay đổi volume từ luồng gốc
      + "+5dB", "-10dB": thay đổi 1 lượng)
      + 0.5, 2 : giảm 1 nửa, gấp đôi
      + "loudnorm": tự động theo chuẩn EBU R128
    + ***language***: [2 characters string] en, vi, fr...
    + ***group***: [string] group id
    + ***label***: [string] display in player
  - **video**:
    + ***fps***: [20, 25(default), 30, 29.97, 59.94, 60]
    + ***codec***: [h264(default), hevc]
    + ***pixelFormat***: [yuv420p(default), yuv444p, yuv420p10le, yuv444p10le]
    + ***width, height***: [0, height], [width, 0], [width, height]
    + ***bitrate, minrate, maxrate, bufsize***: config bitrate
    + ***scaleType***: [fitWidth, fitHeight, fitInside, fitCrop, fitBoth, source]
       (chú ý: nếu để width, height, scaleType trong option tổng, nguồn sẽ scale trước khi xử lý tiếp theo)
    + ***cbr***: [0, 1], bật chế độ constant bitrate
    + ***cq***: [0 -> 51], chế độ constant quality
    + ***interlaced***: [0, 1], chế độ video interlaced
    + ***watermark***: watermark riêng cho profile
    + ***profile, level***: [baseline(cho gpu), main 3.0 | 4.0, high 4.0 | 4.1 | 4.2]
    + ***preset***:
       + cpu: [veryfast, superfast]
       + gpu: [llhp, llhq]
    + ***rc***(chỉ cho gpu): [cbr_hq, cbr_hp] cho cbr, [vbr_hq, vbr_hp] cho vbr, rate control ghi cho preset
    + ***iframe***: đặt iframe
       + -1: tự động detect dựa vào manifest.ts
       + 0 : theo nguồn
       + [1->16]: số giây
       + [17->300]: số frame
    + ***hdr***(nên dùng pixelFormat yuv420p10le, codec hevc): [copy, none, hdr10, hlg10, pq10] hỗ trợ output hdr, hiện tại chỉ hỗ trợ mp4 container(dash hoặc hls+fmp4)
  - **data**: chỉ hỗ trợ copy codec
* **target**:
  - ***format***: [rtmp, udp, hls, dash, mss, hds, mp3, mp4]
  - ***select***: cấu hình profile output, mỗi phần tử có thể là profileId, profileId array, object
    + profileId: dùng cho các truờng hợp đơn giản như dash, mss
    + profileId array: dùng khi muốn gom nhóm audio, video cho hls, hds, ví dụ: 
          select:[
            [
              "480p",
              "480p-audio"
            ]
          ]
    + object: dùng khi muốn cấu hình chi tiết cho từng profileId hoặc profileId array, khi đó object sẽ chứa select riêng và object drm nếu có, ví dụ:
          select:[
            {
              "title":"VTV3",  // Tiêu đề kênh cho udp
              "id": 15,        // programId cho udp
              "drm":{
                "server":"http://drmproxy.com",
                "filter":"widevine+playready+fairplay"
              },
              "select": [
                "480p",
                "480p-audio"
              ]
            }
          ]
  - ***drm***: cấu hình drm
    + type:
      AES_128: hls
      SAMPLE_AES: hls ts(fairplay)
      SAMPLE_AES_CTR, SAMPLE_AES_CBC: hls fmp4, dash
    + filter: dùng cho multidrm, lọc các loại drm system cho manifest
    + dùng trực tiếp:
      + ***uri***: key uri cho hls
      + ***key, keyId, iv***: base64
      + ***widevine, playready***: base64 của pssh tương ứng
      + ***expireTime***: số giây sẽ lấy lại thông tin key(dùng cho proxy mode)
    + dùng proxy: dữ liệu trả về dạng {data: {drm info khi dùng trực tiếp}}
      + ***server***: url để lấy thông tin
      + ***body***: body cho request đến server
  - ***manifest***(cho dash, hls)
    + ***ts***: [2 -> 10], ts duration
    + ***enableTime***(hls): [0, 1], chèn timestamp vào playlist
    + ***appendList***(hls): [0, 1], nối tiếp vào playlist cũ
    + ***endList***(hls): [0, 1], chèn tag endlist khi kết thúc
    + ***cache***: [-1 -> n] số file ts giữ lại khi xóa khỏi playlist, -1 là giữ lại hết
    + ***count***: [0 -> n], số ts giữ trong file playlist, 0 là giữ lại toàn bộ
    + ***singleFile***: [0, 1], bật chế độ không chia nhỏ file
    + ***fmp4***: [0, 1], dùng định dạng fmp4 cho segment
    + Quản lý tên, đường dẫn:
      + Pattern hỗ trợ:
        + ***%%d, %%06d***: index của ts
        + ***%%b***: bitrate của profile(chỉ dùng cho mss, hds)
        + ***%v***: tên của profile video(hoặc audio nếu không có video)
        + ***%%r***: thêm số random
        + ***%%t***: thêm startPts của ts, chỉ dùng cho dash tsName
        + ***., ..***: đường dẫn tương đối từ url chính 
      + ***masterName, masterPath, tsName, tsPrefix, initName, initPrefix, initPath***
      + ***indexName, indexPath, indexPrefix***: chỉ cho dash
      + mss chỉ hỗ trợ %%b, %%d
      + hds tsName bắt buộc phải là Seg1-Frag%%d, đồng thời tsPrefix phải khác rỗng(để tránh lỗi trên một số player), nên dùng: tsPrefix="%v/" tsPath "./%v/" 

    + ***persistent***: [0, 1], giữ kết nối http khi ghi file
    + ***masterExtra***: [string array ngăn cách bởi dấu phảy], ghi thêm file master ra một số tên khác
    + ***chunked***: [0.01 -> n], số giây của chunk khi dùng lowlatency hoặc mp4
    + ***chunkedType***: ["fragment","range", "combine"], loại low latency chia nhỏ hoặc byte-range, default: fragment
    + ***segment***: [0 -> n], số giây cắt đoạn video khi dùng format mp4
    + ***targetLatency***: [0 -> n], số giây latency cho lowlatency 
    + ***minLatency***: [0 -> n], số giây min latency cho lowlatency(dash)
    + ***maxLatency***: [0 -> n], số giây max latency cho lowlatency(dash)
    + ***header***: custom object for header
* **watermark**: tọa độ, kích thước wm sẽ base trên 1920x1080
  - ***x, y***: [0 -> 1920, 0 -> 1080]tọa độ trên video
  - ***id***: định danh để bật tắt
  - ***scale***: [0.1 -> 3.0], scale kích thước watermark + tọa độ nếu cần
  - ***delay***: [0 -> 120], số giây dừng giữa 2 lần loop, dùng cho watermark động
  - ***enable***: [0, 1], ẩn hoặc hiện, có thể điều khiển bằng api 
* **thumbnail**
  - ***width***: [100 -> 1920]
  - ***delay***: [1 -> 60], thời gian giữa 2 ảnh
  - ***col, row***: [0 -> 10], dùng tạo dạng tile cho thumb, mặc định tắt

* **option**
  - ***syncCopy***: [0 - > 2], default 0(disable), đồng bộ luồng copy và transcode, 1 = drop non-idr keyframe, 2 = all frame
  - ***timeout***: [2 -> 10], default 6, ngắt luồng nếu bị dừng ở input, output, transcode
  - ***syncStream***: [1 -> n], default 1, ngắt luồng nếu tiếng và hình lệch nhau
  - ***gpu***: [-1, n], ghi đè gpu ở config
  - ***filterComplex***: custom filter cho transcode(có thể dùng giao diện kéo thả trên cms để tạo)
  - ***xerror***: [0, 1] reset kênh nếu phát hiện gói tin có khả năng gây lỗi
  - ***maxSpeed***: [0.1 -> 10], (default 1.1), tốc độ kênh tối đa
  - ***loop***: [0, n], số lần loop file
  - ***limitInput***: [256 -> 2048], default 1024, số gói tin hàng đợi tối đa khi xử lý bị chậm
  - ***copyts***: [0, 1], copy timestamp từ luồng gốc, mặc định bật cho kênh chỉ băm
  - ***fixAac***: [0, 1], xử lý gói tin aac bị lỗi(gây dừng player trên smarttv)
  - ***sound***: file âm thanh khi chạy pre event bằng image
  - ***preload***: [0, 1], chuẩn hóa lại video trước khi chạy pre event bằng video
  - ***id3***: [0, 1], add luồng id3 vào các output hls ts
  - ***s3Config***: đuờng đẫn đến file s3 config(s3 url có dạng s3://path/to/file), mặc định là data/s3.json, nội dung file như sau:
```json
{
   "accessKeyId": "<required>",
   "secretAccessKey": "<required>",
   "bucket": "<required>",
   "endpoint": "<optional>",
   "region": "<optional>"
}
```

***info***: cấu hình như text của ads
### 3. Tắt kênh

*** Request***

```json
{
  "type": "stop",
  "name": "vtv3"
}
```

***Response***

```json
{
  "ec": 0,
  "result": {"vtv3": "OK"},
  "dt": 4,
  "msg": "Stop 1 task(s) \"vtv3\""
}
```

### Lấy danh sách kênh

**Request**: truyền vào tên, nếu lấy tất cả thì để rỗng

```json
{
  "type": "progress",
  "name": ""
}

```

**Response**:

```json
{
  "total": 1,
  "ec": 0,
  "result": {
    "transcoder": {
      "speed": 1.0,
      "created": "2020-08-13 11:23:39",
      "startup": "2020-08-13 11:50:05",
      "state": "started",
      "life": "1h41m18s",
      "pts": "1h41m19s",
      "error": [
        "[08-13 11:48:24] Input timeout (code: INPUT_TIMEOUT)"
      ],
      "input": [
        {
          "url": "udp://234.5.6.7:8888",
          "profile": [
            "aac stereo@48000 137Kbps lc",
            "h264 1280x720p@25 vbr yuv420p high 3.1"
          ],
          "bitrate": 2390646
        }
      ],
      "profile": [
        {
          "id": "128k",
          "bitrate": 128760
        }
        {
          "id": "480",
          "bitrate": 962192
        }
      ],
      "target": [
        {
          "format": "hls",
          "url": "/mnt/ramdisk/origin04/source/",
          "select": [
            "128k",
            "480"
          ],
          "bitrate": 5211223
        }
      ],
      "weight": 1,
      "_id": ""
    }
  },
  "dt": 2
}
```

### 5. Lấy thông tin máy

**Request**

```
{
  "type":"dump"
}
```

**Response**

```json
{
  "name": "Transcode-172-16-60-226",
  "version": "2.0.2",
  "total": 0,
  "speed": 1.0,
  "build": "2020-07-28 10:58:24",
  "start": "2020-07-28 10:59:09",
  "queue": 0,
  "system": {
    "os": "CentOS Linux 7 (Core)",
    "cores": 48,
    "ramTotal": 128320,
    "ramUsed": 123934,
    "swapTotal": 8191,
    "swapUsed": 1642,
    "network": [
      {
        "name": "em4.530",
        "ip": "172.16.30.226"
      }
    ],
    "cpu": 13,
    "gpu": [
      {
        "index": 0,
        "name": "Quadro P6000",
        "total": 24449,
        "used": 2444,
        "gpu": 42,
        "mem": 10,
        "enc": 51,
        "dec": 45,
        "percent": 0,
        "count": 0,
        "speed": 1.0
      }
    ]
  },
  "ec": 0,
  "result": {},
  "dt": 367
}
```

###  6. Update

* ***name***: tên kênh 
* ***filter***: overlay, volume, drm, header
  - ***overlay***: điều khiển bằng id trong config
  - ***volume***: điều khiển bằng id của profile
  - ***drm***, ***header***: điều khiển bằng id là key của object 'changed'

***Request***

```json
{
  "type": "update",
  "name": "vtv3",
  "filter": "overlay",
  "id": "0",
  "changed": {
    "x": 300,
    "enable": 1
  }
}
```

***Response***

```json
{
  "ec": 0,
  "result": {},
  "dt": 1
}
```

### 7. Event

* Để sử dụng api này cần bật luồng id3: ***option.id3 = 1***

***Request*** 

```json
{
  "type": "event",
  "name": "<task name>",
  "content": {
    "TXXX": "test txxx",
    "TEXT": "test text"
  }
}

```

***Response***

```json

{
  "ec": 0,
  "result": {},
  "dt": 0
}

```

### 8. Ads

* Để sử dụng api này cần bật slot vẽ: ***root.ads = [{}{}...]***, là mảng các object config
* Mỗi object sẽ có các option sau
  - ***id***: [string], định danh dùng để điều khiển
  - ***url***: [string], đường dẫn ảnh hoặc video
  - ***x, y***: [int], tọa độ vẽ, theo hệ quy chiếu graphics, tính từ góc trên trái 
  - ***type***: [string]
    + ***countdown***: đếm lùi đến thời gian ${timer}, text sẽ có dạng "Content %s" sẽ hiện lên thành "Content 01:20:00"
    + ***marquee***: vẽ text chạy qua màn hình
    + ***label***: vẽ text cố định trên màn hình
    + ***logo***: vẽ 1 video hoặc image lên màn hình
    + ***background***: co màn hình lại, vẽ quảng cáo phía dưới hoặc ngược lại(tùy vào giá trị của percent), lưu ý, quảng cáo sẽ bị scale stretch theo kích thước video
  - ***align***: [string], xác định điểm trên video để vẽ, sau đó cộng trừ tọa độ x, y, kết hợp của các string: left, top, right, bottom, center, ví dụ neo ở giữa, bên duới màn hình: bottom+center 
  - ***loop***: [-1 -> n], số lần lặp, -1 là lặp vô hạn(thường kết hợp với duration), 0 là chỉ chạy 1 lần, không lặp lại
  - ***delay***: [int], số giây delay giữa 2 lần lặp, default 0
  - ***enable***: [0, 1], bật tắt
  - ***duration***: thời gian hiện quảng cáo, default 0 = no limit(khi đó thời gian hiện phụ thuộc vào loop)

  ***@not for background***
  - ***scale***: scale nội dung quảng cáo, font size, tọa độ, default: video-height/1080

  ***@for background***
  - ***percent***: [-1 -> 1], phần trăm width nội dung quảng cáo khi dùng chế độ background, nếu là số âm, quảng cáo sẽ được co lại và vẽ lên trên

  ***@for countdown, marquee, label***
  - ***text***: text để vẽ
  - ***size***: [0 -> 50], font size of text , default 30
  - ***border***: [0, 5]. border size of text in pixel, default 0
  - ***box***: [0 -> 50], padding của box khi vẽ text, default 0
  - ***alpha***: [0, 255], opacity của box, default 0
  - ***shadowX, shadowY***: [-4, 4], shadow của text in pixel, default 0
  - ***lineSpace***: [0, 50], khoảng cách dòng khi vẽ text in pixel, default 0

  ***@for countdown***
  - ***timer***: thời gian kết thúc bằng unix time khi dùng chế độ Countdown
#### Để điều khiển quảng cáo:

***Request*** 

```json
{
  "type": "ads",
  "name": "<channel name>",
  "id": "<ads id>",
  "content": {
    "text": "blackout.jpg",
    "type": "background",
    "percent": 0.0,
    "x": 0,
    "align": "center"
    ...
  }
}

```

***Response***

```json

{
  "ec": 0,
  "result": {},
  "dt": 0
}

```



### 9. Scte-35

* Để sử dụng api này cần bật luồng scte35: ***option.scte35 = 1***
- ***eventId***: [int], định danh của event, mặc định 0
- ***cueOut***: [0, 1], đánh dấu là cueOut event, mặc định 1
- ***duration***: [int], số giây cueOut, sau đó event sẽ tự động cueIn, mặc định 0

***Request*** 

```json
{
  "type": "scte35",
  "name": "<task name>",
  "content": {
    "eventId": 123,
    "cueOut": 1,
    "duration": 200
  }
}

```

***Response***

```json

{
  "ec": 0,
  "result": {},
  "dt": 0
}

```

### 10. Mixer

* Cấu hình input ở dạng json object

```
"input": {
    "type": "mix",
    "bgr":"background.jpg",
    "list": [
      {
        "url": "udp://234.5.6.7:8888",
        "x": 50,
        "y": 50,
        "width": 300,
        "height": 300,
        "ratio": 1,
        "zorder": 1
      }
    ],
    "width": 1280,
    "height": 720,
    "fps": 25
  }
```
- ***list***: [object array], danh sách input, có thể thay đổi được
   + ***x, y, width, height***: [int], ô chứa video, default 0 = không hiện video, chỉ trộn tiếng nếu có
   + ***ratio***: [0->1], chế độ scale từ centerInside đến centerCrop, default 1
   + ***zorder***: [int], thứ tự hiển thị
- ***width, height, fps***: [int], thông tin video dùng để vẽ các input lên
- ***bgr***: [string], url của hình ảnh hoặc video nền(luôn luôn vẽ trước các input)

* Cập nhật input dùng lệnh update

```
{
  "type": "update",
  "name": "<task name>",
  "filter": "virtual",
  "changed": {
    "list": [
      "rtmp://172.16.60.226/live/5001"
      "rtmp://172.16.60.226/live/5002"
    ]
  }
}
```

- ***join***: [object], cấu hình đặt trong target
   + ***callback***: [string], url gọi callback
   + ***maxSpeed***: [double], tốc độ tối đa, default 100
   + ***clean***: [int], dọn file con sau khi join

* Callback upload dạng json, nếu có lỗi status = error

```
{
  "path": "/mnt/ramdisk/record/",
  "callback": "http://localhost:8888/callback.txt",
  "speed": "50.00",
  "clean": 0,
  "status": "success",
  "joined": "/mnt/ramdisk/record/joined.mp4",
  "info": [
    {
      "id": "0",
      "type": "audio",
      "codec": "aac",
      "bitrate": 128527,
      "duration": 30.551333,
      "channel": "stereo",
      "sampleRate": 48000,
      "profile": "lc"
    },
    {
      "id": "1",
      "type": "video",
      "codec": "h264",
      "bitrate": 2733013,
      "duration": 30.666667,
      "startTime": 0.066667,
      "width": 1920,
      "height": 1080,
      "fps": "30",
      "pixelFormat": "yuv420p",
      "bframe": 2,
      "profile": "high",
      "level": "4.1"
    }
  ],
  "segments": [
    {
      "name": "tmp.mp4",
      "duration": 0.0,
      "used": 0,
      "reason": "Can't read info"
    },
    {
      "name": "20211025103819.mp4",
      "duration": 30.733334,
      "used": 1
    }
  ]
}
```

### 11. Apps

 - Để sử dụng, cần enable app tương ứng trong config
```
  ...
   "nginx": 1,
   "srs": 1
  ...
```
 - ***Cấu hình***: trong thư mục apps/<app_name>/data
 - ***Lệnh điều khiển***: gọi qua http url
 ```
   http://<ip>:<port>/apps/<app_name>/<action>
   + <app_name>: srs hoặc nginx
   + <action>: start, stop, reset, status
 ```
### 12. Stream

* Cấu hình input ở dạng json object

```
  "input": {
    "type": "sigma",
    "list": [
      {
        "id": "first",
        "url": "input.mp4",
        "start": 0,
        "end": 60,
        "show": "HH:mm:ss dd-MM-yyyy",
        "duration": 60
      }
    ],
    "width": 1280,
    "height": 720,
    "fps": 25
  }
```
- ***list***: [object array], danh sách input, có thể thay đổi được, mỗi input phải có video + audio
   + ***id, url***: [string required], id, path of video
   + ***start, end***: [0->n], cắt video nguồn, default: 0, không cắt
   + ***show***: [string], thời điểm phát, default: ‘’, phát ngay khi có thể
   + ***duration***: [0->n], thời gian phát, default: 0, nếu duration > video duration thì sẽ loop
- ***width, height, fps***: [int], thông tin video dùng để vẽ input lên

### 13. Subtitle

- ***Supported***: transcode only
   + DVBTeletext(Text)  -> webvtt(hls, dash), ttml-text(dash), select teletext page by ***profile.page***(ex: 778) or ***profile.language***(ex: eng)
   + DVBSubtitle(Image) -> ttml-image(dash)
- ***Format webvtt***: in player css
```
       video::cue {
          background: transparent;
          color: white;
          text-shadow: black 0px 0px 1px;
       }
```
- ***Format ttml-text***: in ***profile.cfg*** object
     + ***font***: font family, default: sansSerif
     + ***size***: font size, default: 80%
     + ***color***: font color, default: white
     + ***outSize***: text outline size, default: 5%
     + ***outColor***: text outline color, default: black
     + ***bgr***: background color, default: transparent

### 14. Audio watermark

- ***Config***: use profile.awt5 of audio
```
    {
        "wm":"0x999999"
        "preset": 0,
        "opt": "-bottom_freq 1750 -top_freq 11000 -framesync_freq 2500 -payload_frames 1 -crc_percent 20 -aggressiveness 0.75 -emboss_gain -40 -emphasis_gain 6"
    }
```

     + ***wm***: watermark payload in hex string
     + ***preset***:
        -1: auto detect for watermark byte length
        0: disable, use custom in opt field
        1: recommended for 1 bytes long payload, using 1 data frames
        2: recommended for 2 bytes long payload, using 1 data frames
        3: recommended for 2 bytes long payload, using 2 data frames
        4: recommended for 3 bytes long payload, using 1 data frames
        5: recommended for 3 bytes long payload, using 2 data frames
        6: recommended for 4 bytes long payload, using 2 data frames
        7: recommended for 4 bytes long payload, using 3 data frames
        8: recommended for 6 bytes long payload, using 3 data frames
        9: recommended for 6 bytes long payload, using 4 data frames
        10: recommended for 8 bytes long payload, using 4 data frames
     + ***opt***: custom options when not use preset

Install, using ffi-napi and ref-napi
```sh
npm install ffi-napi
npm install ref-napi
```

[//]: # (These are reference links used in the body of this note and get stripped out when the markdown processor does its job. There is no need to format nicely because it shouldn't be seen. Thanks SO - http://stackoverflow.com/questions/4823468/store-comments-in-markdown-syntax)

   [ffi-napi]: <https://www.npmjs.com/package/ffi-napi>
   [ref-napi]: <https://www.npmjs.com/package/ref-napi>

