Libraries:

* libavcodec (decode),   2015/3/8
  multimedia library
* mpg123 (decode, LGPL), 2016/2/23, o
* mad (decode, GPL),     2004/2/18
  too old
* minimp3 (decode),      2007/2/5
* lame (decode, LGPL),   2012/2/28
* sfml (decode + play)
  no mp3
  C++
* soundio (play),        2016/2/18
  no msvc
* libao (play),          2014/1/27
  no build tutorial
* PortAudio (play),      2014/1/30, o
* OpenAL (decode + play)
  mp3 via extension
* gstreamer (decode + play)
  multimedia framework
* libcanberra
  baby
* libsndfile
  wav only

Examples:

* mpg123 + libao
  http://hzqtc.github.io/2012/05/play-mp3-with-libmpg123-and-libao.html
* mad + PulseAudio (no Windows)
  http://lauri.xn--vsandi-pxa.com/2013/12/implementing-mp3-player.en.html

Installation:

* Ubuntu

    ```shell
    $ sudo apt-get install libjack-jackd2-dev portaudio19-dev
    ```

Non-blocking:

* http://stackoverflow.com/questions/18236020/should-i-be-using-gtk-threads-good-tutorials-on-gtk-threading

    ```C
    while (gtk_events_pending())
      gtk_main_iteration();
    ```
