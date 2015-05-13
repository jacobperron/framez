# framez 
Can send video frames using a pub/sub pattern and show timings (frame transfer, encoding, decoding time, etc.)

### Requirements: [OpenCV](http://opencv.org/downloads.html) (tested with 2.4.10) and [ZMQ](http://zeromq.org/area:download) (4.0.5)

#### ZMQ install instructions (Ubuntu):
	
	tar -xvf zeromq-4.0.5.tar.gz
	cd zeromq-4.0.5
	sudo ./configure
	sudo make
	sudo make install
	sudo ldconfig

### How to use:
Using publisher:

    make pub
    ./framepub [device_idx]

Using subscriber:

    make sub
    ./framesub [pub_hostname]

### TODO
* Wrap code in classes. 
