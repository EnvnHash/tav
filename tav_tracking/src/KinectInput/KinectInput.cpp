//
//  KinectInput.cpp
//  KinectInput
//
//  Created by Sven Hahne on 13.04.14.
//  Copyright (c) 2014 Sven Hahne. All rights reserved.
//
//  when close range is set to 1 the brightness of the ir camera is way lower

#include <KinectInput/KinectInput.h>

namespace tav
{

KinectInput::KinectInput(std::string _path, kinPar* _kPar, kinectMapping* _kMap) :
		kPar(_kPar), kMap(_kMap), recPath(_path)
{
	playRecorded = true;

	getStream = new bool[KI_NR_STREAMS];
	for (int i = 0; i < KI_NR_STREAMS; i++)
		getStream[i] = false;

	if (kPar->useDepth) getStream[KI_DEPTH] = true;
	if (kPar->useColor) getStream[KI_COLOR] = true;
	if (kPar->useIr) 	getStream[KI_IR] = true;

	// check if file exists
	if (access(_path.c_str(), F_OK) != -1)
		init();
	else
		printf("KinectInput Error: oni file not found!!! \n");
}

//---------------------------------------------------------------

KinectInput::KinectInput(bool _getDepth, bool _getColor, kinectMapping* _kMap) :
		kMap(_kMap)
{
	playRecorded = false;

	kPar = new kinPar();
	kPar->emitter = true;

	getStream = new bool[KI_NR_STREAMS];
	for (int i = 0; i < KI_NR_STREAMS; i++)
		getStream[i] = false;

	if (_getDepth)
		getStream[KI_DEPTH] = true;
	if (_getColor)
		getStream[KI_COLOR] = true;

	init();
}

//---------------------------------------------------------------

KinectInput::KinectInput(kinPar* _kPar, kinectMapping* _kMap) :
		kPar(_kPar), kMap(_kMap)
{
	playRecorded = false;
	getStream = new bool[KI_NR_STREAMS];
	for (int i = 0; i < KI_NR_STREAMS; i++)
		getStream[i] = false;

	if (kPar->useDepth)
		getStream[KI_DEPTH] = true;
	if (kPar->useColor)
		getStream[KI_COLOR] = true;
	if (kPar->useIr)
		getStream[KI_IR] = true;

	init();
}

//---------------------------------------------------------------

int KinectInput::init()
{
	generateMips = false;
	bIsReady = false;

	// mapping parameters
	kMap->distScale->x = 5050.f;
	kMap->distScale->y = 6050.f;
	kMap->offset->x = 0.f;
	kMap->offset->y = 0.1f;
	kMap->scale->x = 1.f;
	kMap->scale->y = 1.f;

	// init ir histogram buffers
	histoPar = new HistoPar();
	histoPar->pHist = new float*[histoPar->nrBuffers];
	for (unsigned short i = 0; i < histoPar->nrBuffers; i++)
		histoPar->pHist[i] = new float[histoPar->maxIrBright];

	// openni init
	openni::Status rc = openni::STATUS_OK;
	rc = openni::OpenNI::initialize();
	static const char* err = openni::OpenNI::getExtendedError();

	// get all active devices
	openni::OpenNI::enumerateDevices(&deviceList);
	if (!playRecorded)
		nrDevices = deviceList.getSize();
	else
		nrDevices = 1;

	std::vector<unsigned int> cleanDevList;

	// if there are more than 1 device check if there is a Kinect v2 which comes 3 devices (for different depth resolutions)
	if (nrDevices > 0)
	{
		// check if it's a freenect
		for (short i = 0; i < nrDevices; i++)
		{
			// interpret the format std::string
			std::string strFormat = std::string(deviceList[i].getUri());
			std::vector<std::string> sepFormat = split(strFormat, ':');

			// if it's a freenect2, take the second device (freenect2://0?depth-size=640x480 ) which is compatible with NiTE
			if (static_cast<unsigned short>(sepFormat.size()) > 1
					&& std::strcmp(sepFormat[0].c_str(), "freenect2") == 0)
			{
				cleanDevList.push_back(i + 1);
				i += 2;
			}
			else
			{
				cleanDevList.push_back(i);
			}
		}
	}

	nrDevices = cleanDevList.size();

	std::cout << "found " << nrDevices << " openni devices" << std::endl;

	device = new openni::Device[nrDevices];
	devicePresent = new bool[nrDevices];
	streams = new openni::VideoStream*[nrDevices];
	streamPars = new ki_StreamPar*[nrDevices];
	frameListeners = new KinectStreamListener**[nrDevices];
	pPlaybackControl = new openni::PlaybackControl*[nrDevices];
	nis = new NISkeleton*[nrDevices];
	lastUploadDepthFrame = new int[nrDevices];
	lastUploadDepthFrameNoHisto = new int[nrDevices];
	lastUploadIrFrame = new int[nrDevices];
	lastUploadColFrame = new int[nrDevices];

	for (short i = 0; i < nrDevices; i++)
	{
		if (debug)
			std::cout << "device[" << i << "] name:"
					<< deviceList[cleanDevList[i]].getName() << ", uri:"
					<< deviceList[cleanDevList[i]].getUri()
					<< ", usbProductId: "
					<< deviceList[cleanDevList[i]].getUsbProductId()
					<< ", usbVendorId:"
					<< deviceList[cleanDevList[i]].getUsbVendorId()
					<< std::endl;

		streams[i] = new openni::VideoStream[KI_NR_STREAMS];
		streamPars[i] = new ki_StreamPar[KI_NR_STREAMS];

		frameListeners[i] = new KinectStreamListener*[KI_NR_STREAMS];
		for (short j = 0; j < KI_NR_STREAMS; j++)
			frameListeners[i][j] = nullptr;

		if (playRecorded)
		{
			printf("KinecInput is playing back oni file \n");
			rc = device[i].open(recPath.c_str());

			if (rc != openni::STATUS_OK)
				printf("KinectInput Error: Couldn´t open oni file!!! \n");

			for (int strm = 0; strm < KI_NR_STREAMS; strm++)
			{
				if (getStream[strm])
				{
					const openni::SensorInfo* sensInf = device[i].getSensorInfo(
							static_cast<openni::SensorType>(strm + 1));
					const openni::Array<openni::VideoMode>& modes =
							sensInf->getSupportedVideoModes();

					streamPars[i][strm].mode.setFps(modes[0].getFps());
					streamPars[i][strm].mode.setPixelFormat(
							modes[0].getPixelFormat());
					streamPars[i][strm].mode.setResolution(
							modes[0].getResolutionX(),
							modes[0].getResolutionY());
				}
			}
		}
		else
		{
			streamPars[i][KI_COLOR].mode.setFps(kPar->colorFps);
			streamPars[i][KI_COLOR].mode.setPixelFormat(kPar->colorFmt);
			streamPars[i][KI_COLOR].mode.setResolution(kPar->colorW,
					kPar->colorH);

			streamPars[i][KI_DEPTH].mode.setFps(kPar->depthFps);
			streamPars[i][KI_DEPTH].mode.setPixelFormat(kPar->depthFmt);
			streamPars[i][KI_DEPTH].mode.setResolution(kPar->depthW,
					kPar->depthH);

			streamPars[i][KI_IR].mode.setFps(kPar->irFps);
			streamPars[i][KI_IR].mode.setPixelFormat(kPar->irFmt);
			streamPars[i][KI_IR].mode.setResolution(kPar->irW, kPar->irH);

			const char* uri = deviceList[cleanDevList[i]].getUri();

			rc = device[i].open(uri);
			//device[i].setDepthColorSyncEnabled(true);
		}

		devicePresent[i] = true;
		device[i].setDepthColorSyncEnabled(true);

		if (rc != openni::STATUS_OK)
		{
			printf("KinectInput: Device %d open failed:\n%s\n", i,
					openni::OpenNI::getExtendedError());
			openni::OpenNI::shutdown();
			devicePresent[i] = false;
		}

		//---------------- Init streams ------------------------------------------------------------------------

		if (devicePresent[i])
		{
			setImageRegistration(kPar->registration, i);

			for (int strm = 0; strm < KI_NR_STREAMS; strm++)
			{
				if (getStream[strm])
				{
					if (debug)
					{
						switch (strm)
						{
						case 0:
							std::cout << "trying to get IR Stream" << std::endl;
							break;
						case 1:
							std::cout << "trying to get COLOR Stream" << std::endl;
							break;
						case 2:
							std::cout << "trying to get DEPTH Stream" << std::endl;
							break;
						default:
							break;
						}
					}

					const openni::SensorInfo* sensInf = device[i].getSensorInfo(static_cast<openni::SensorType>(strm + 1));
					const openni::Array<openni::VideoMode>& modes = sensInf->getSupportedVideoModes();
					bool found = false;

					if (!playRecorded)
					{
						for (int k = 0; k < modes.getSize(); k++)
						{
							if (debug)
							{
								std::cout << "available videoFormat[" << k << "]: fps:" << streamPars[i][strm].mode.getFps() << ", pixFmt";
								std::cout << streamPars[i][strm].mode.getPixelFormat() << ", width: ";
								std::cout << streamPars[i][strm].mode.getResolutionX() << ", height: ";
								std::cout << streamPars[i][strm].mode.getResolutionY() << std::endl;
							}

							if (modes[k].getFps() == streamPars[i][strm].mode.getFps()
									&& modes[k].getPixelFormat() == streamPars[i][strm].mode.getPixelFormat()
									&& modes[k].getResolutionX() == streamPars[i][strm].mode.getResolutionX()
									&& modes[k].getResolutionY() == streamPars[i][strm].mode.getResolutionY())
							{
								found = true;
							}
						}

						if (debug)
							std::cout << std::endl;

						if (!found)
							printf(
									"Error: video format not found for stream %d\n",
									strm);

						rc = streams[i][strm].create(device[i], static_cast<openni::SensorType>(strm + 1));
						streams[i][strm].setVideoMode(streamPars[i][strm].mode);
					}
					else
					{
						rc = streams[i][strm].create(device[i], static_cast<openni::SensorType>(strm + 1));
						streams[i][strm].setVideoMode(streamPars[i][strm].mode);
					}

					// if color stream
					if (strm == KI_COLOR && !playRecorded)
					{
						setImageAutoExposure(kPar->autoExp, i);
						setImageAutoWhiteBalance(kPar->autoWB, i);
						setColorMirror(kPar->mirror);
						//        setImageExposure(50);
						//        setImageGain(50); // 0 - 50
					}

					// if depth stream
					if (strm == KI_DEPTH)
					{
						if (!playRecorded)
						{
							setCloseRange(kPar->closeRange);
							setDepthMirror(kPar->mirror);

/*
							// get nite relevant paramenters
							char* pData = new char[8];
							int dataSize = 8;
							streams[i][strm].getProperty( XN_STREAM_PROPERTY_CONST_SHIFT, (void*)pData, &dataSize );
							std::cout << "XN_STREAM_PROPERTY_CONST_SHIFT dataSize: 8" ;
							for(unsigned int i=0; i<dataSize; i++) std::cout << pData[0];
							std::cout << std::endl;
*/

						} else
						{
							/*
							// if we are loading a oni file recorded with NiViewer there are some Parameters missing
							// that make NiTE crash. we are adding them here manually
							std::cout << "setting depth stream property" << std::endl;
							//char* pData = new char[8];
							char csData[8] = "��������";

							streams[i][strm].setProperty(XN_STREAM_PROPERTY_CONST_SHIFT, csData, 8);
							streams[i][strm].setProperty(XN_STREAM_PROPERTY_PARAM_COEFF, 8);
							streams[i][strm].setProperty(XN_STREAM_PROPERTY_SHIFT_SCALE, 8);
							streams[i][strm].setProperty(XN_STREAM_PROPERTY_MAX_SHIFT, 8);
							streams[i][strm].setProperty(XN_STREAM_PROPERTY_S2D_TABLE, 4096);
							streams[i][strm].setProperty(XN_STREAM_PROPERTY_D2S_TABLE, 20002);
							streams[i][strm].setProperty(XN_STREAM_PROPERTY_ZERO_PLANE_DISTANCE, 8);
							streams[i][strm].setProperty(XN_STREAM_PROPERTY_ZERO_PLANE_PIXEL_SIZE, 8);
							streams[i][strm].setProperty(XN_STREAM_PROPERTY_EMITTER_DCMOS_DISTANCE, 8);

							int val;
							streams[i][strm].getProperty(XN_STREAM_PROPERTY_ZERO_PLANE_DISTANCE, &val);
							std::cout << " reading back XN_STREAM_PROPERTY_ZERO_PLANE_DISTANCE "<< val << std::endl;
*/
						}
					}

					int size = sizeof(float);
					streams[i][strm].getProperty( ONI_STREAM_PROPERTY_HORIZONTAL_FOV, &kPar->depthFovX, &size);
					streams[i][strm].getProperty( ONI_STREAM_PROPERTY_VERTICAL_FOV, &kPar->depthFovY, &size);

					// wenn eine zweite kinect dranhaengt funktionieren ab hier die listener nicht mehr...
					if (rc == openni::STATUS_OK)
					{
						rc = streams[i][strm].start();
						if (rc != openni::STATUS_OK)
						{
							printf("KinectInput: Couldn't start stream %d:\n%s\n",
									strm, openni::OpenNI::getExtendedError());
							streams[i][strm].destroy();
						}
					}
					else
					{
						printf("KinectInput: Couldn't find stream %d :\n%s\n",
								strm, openni::OpenNI::getExtendedError());
					}

					printf("Device[%d], Mode [%d]: fps: %d pixFmt: %d resX: %d resY: %d \n",
							i, strm, streamPars[i][strm].mode.getFps(),
							streamPars[i][strm].mode.getPixelFormat(),
							streamPars[i][strm].mode.getResolutionX(),
							streamPars[i][strm].mode.getResolutionY());

					getGlFormat(&streamPars[i][strm]);

					switch (strm)
					{
					case KI_IR:
						frameListeners[i][KI_IR] = new KinectIrStreamListener(histoPar, kPar->irAmp);
						streams[i][KI_IR].addNewFrameListener(frameListeners[i][KI_IR]);
						lastUploadIrFrame[i] = -1;
						break;
					case KI_COLOR:
						frameListeners[i][KI_COLOR] = new KinectColorStreamListener(streamPars[i][KI_COLOR].nrChans);
						streams[i][KI_COLOR].addNewFrameListener(frameListeners[i][KI_COLOR]);
						lastUploadColFrame[i] = -1;
						break;
					case KI_DEPTH:
						frameListeners[i][KI_DEPTH] = new KinectDepthStreamListener(streamPars[i][KI_DEPTH].nrChans);
						streams[i][KI_DEPTH].addNewFrameListener(frameListeners[i][KI_DEPTH]);
						lastUploadDepthFrameNoHisto[i] = -1;
						lastUploadDepthFrame[i] = -1;
						break;
					default:
						break;
					}
				}
			}

			// if (!playRecorded) setEmitter(kPar->emitter, i);  // has to be called after streams are started

			// ------ add additional processing stages -------------------
			// ------ stages will be processed in this order -------------
			// add NiTE-Skeleton-Tracking

			if (kPar->useNiteSkel)
			{
				if (debug) printf("KinectInput::init using Nite Skeleton \n");

				nis[i] = new NISkeleton(&device[i], kMap->distScale->y);

				if (frameListeners[i][KI_DEPTH])
				{
					if (debug) printf("KinectInput::init adding Nite Depth Frame Listeners \n");
					((KinectDepthStreamListener*) frameListeners[i][KI_DEPTH])->addPostProc(nis[i]);
				}
			}
		}
	}

	bIsReady = true;

	return 0;
}

//---------------------------------------------------------------

void KinectInput::getGlFormat(ki_StreamPar* strPar)
{
	switch (strPar->mode.getPixelFormat())
	{
	case openni::PIXEL_FORMAT_SHIFT_9_2:
		strPar->nrChans = 1;
		strPar->pixType = GL_UNSIGNED_BYTE;
		break;
	case openni::PIXEL_FORMAT_SHIFT_9_3:
		strPar->nrChans = 1;
		strPar->pixType = GL_UNSIGNED_BYTE;
		break;
	case openni::PIXEL_FORMAT_RGB888:
		strPar->nrChans = 3;
		strPar->intFormat = GL_RGB8;
		strPar->extFormat = GL_RGB;
		strPar->pixType = GL_UNSIGNED_BYTE;
		break;
	case openni::PIXEL_FORMAT_YUV422:
		strPar->nrChans = 3;
		strPar->intFormat = GL_YCBCR_422_APPLE;
		strPar->extFormat = GL_RGB;
		strPar->pixType = GL_UNSIGNED_BYTE;
		break;
	case openni::PIXEL_FORMAT_GRAY8:
		strPar->nrChans = 1;
		strPar->intFormat = GL_R8;
		strPar->extFormat = GL_RED;
		strPar->pixType = GL_UNSIGNED_BYTE;
		break;
	case openni::PIXEL_FORMAT_GRAY16:
		strPar->nrChans = 1;
		strPar->intFormat = GL_R8;
		strPar->extFormat = GL_RED;
		strPar->pixType = GL_UNSIGNED_BYTE;
		//            strPar->intFormat = GL_R16;
		//            strPar->extFormat = GL_RED;
		//            strPar->pixType = GL_UNSIGNED_SHORT;
		break;
	case openni::PIXEL_FORMAT_JPEG:
		strPar->nrChans = 3;
		strPar->intFormat = GL_RGB8;
		strPar->extFormat = GL_RGB;
		strPar->pixType = GL_UNSIGNED_BYTE;
		break;
	case openni::PIXEL_FORMAT_YUYV:
		strPar->nrChans = 4;
		strPar->intFormat = GL_YCBCR_422_APPLE;
		strPar->extFormat = GL_RGB;
		strPar->pixType = GL_UNSIGNED_BYTE;
		break;
	case openni::PIXEL_FORMAT_DEPTH_1_MM:
		if (debug)
			std::cout << "get Gl Format: Gl_R32F, GL_RED, GL_FLOAT"
					<< std::endl;
		strPar->nrChans = 1;
		strPar->intFormat = GL_R32F;
		strPar->extFormat = GL_RED;
		strPar->pixType = GL_FLOAT;
		break;
	case openni::PIXEL_FORMAT_DEPTH_100_UM:
		strPar->nrChans = 1;
		strPar->intFormat = GL_R32F;
		strPar->extFormat = GL_RED;
		strPar->pixType = GL_FLOAT;
		break;
	default:
		printf("KinectInput: texture allocate: pixel format not found \n");
		break;
	}
}

//---------------------------------------------------------------

void KinectInput::allocate(ki_StreamType _type, int deviceNr, void* img,
		GLuint* texId)
{
	// generate color texture
	glGenTextures(1, texId);
	glBindTexture(GL_TEXTURE_2D, *texId);

	if (generateMips)
		mimapLevels = 5;
	else
		mimapLevels = 1;

	// define inmutable storage
	glTexStorage2D(GL_TEXTURE_2D,
			mimapLevels,                 // nr of mipmap levels
			streamPars[deviceNr][_type].intFormat,       // internal format
			streamPars[deviceNr][_type].mode.getResolutionX(),
			streamPars[deviceNr][_type].mode.getResolutionY());

	glTexSubImage2D(
			GL_TEXTURE_2D,              // target
			0,                          // First mipmap level
			0,
			0,                       // x and y offset
			streamPars[deviceNr][_type].mode.getResolutionX(), // width and height
			streamPars[deviceNr][_type].mode.getResolutionY(),
			streamPars[deviceNr][_type].extFormat,
			streamPars[deviceNr][_type].pixType, img);

	// set linear filtering
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	// unbind
	glBindTexture(GL_TEXTURE_2D, 0);
}

//---------------------------------------------------------------

bool KinectInput::isDeviceReady(int deviceNr)
{
	return devicePresent[deviceNr];
}

//---------------------------------------------------------------

bool KinectInput::isReady()
{
	return bIsReady;
}

//---------------------------------------------------------------

bool KinectInput::isNisInited(int deviceNr)
{
	bool out = false;
	if (nis)
		out = nis[deviceNr]->isInited();
	return out;
}

//---------------------------------------------------------------

void KinectInput::uploadImg(ki_StreamType _type, int deviceNr, bool useHisto)
{
	void* ptr = 0;

	if (frameListeners[deviceNr][_type])
	{
		switch (_type)
		{
		case KI_IR:
			ptr = ((KinectIrStreamListener*) frameListeners[deviceNr][_type])->getActImg();
			break;
		case KI_COLOR:
			ptr = ((KinectColorStreamListener*) frameListeners[deviceNr][_type])->getActImg();
			break;
		case KI_DEPTH:
			if (useHisto)
			{
				ptr = ((KinectDepthStreamListener*) frameListeners[deviceNr][_type])->getActImg();
			}
			else
			{
				ptr = ((KinectDepthStreamListener*) frameListeners[deviceNr][_type])->getActImgNoHisto();
			}
			break;
		default:
			break;
		}

		if (frameListeners[deviceNr][_type]->isInited() && ptr != 0)
		{
			streamPars[deviceNr][_type].texPtr = ++streamPars[deviceNr][_type].texPtr % streamPars[deviceNr][_type].nrTexs;

			if (useHisto)
			{
				if (!streamPars[deviceNr][_type].texInited)
				{
					streamPars[deviceNr][_type].texId = new GLuint[streamPars[deviceNr][_type].nrTexs];

					for (unsigned int i=0;i<streamPars[deviceNr][_type].nrTexs; i++)
						allocate(_type, deviceNr, ptr, &streamPars[deviceNr][_type].texId[i]);

					streamPars[deviceNr][_type].texInited = true;
				}

				glBindTexture(GL_TEXTURE_2D, streamPars[deviceNr][_type].texId[ streamPars[deviceNr][_type].texPtr ]);
			}
			else
			{
				if (!streamPars[deviceNr][_type].texInitedNoHisto)
				{
					streamPars[deviceNr][_type].texIdNoHisto = new GLuint[streamPars[deviceNr][_type].nrTexs];

					for (unsigned int i=0;i<streamPars[deviceNr][_type].nrTexs; i++)
						allocate(_type, deviceNr, ptr, &streamPars[deviceNr][_type].texIdNoHisto[i]);

					streamPars[deviceNr][_type].texInitedNoHisto = true;
				}

				glBindTexture(GL_TEXTURE_2D, streamPars[deviceNr][_type].texIdNoHisto[ streamPars[deviceNr][_type].texPtr ]);
			}

			glTexSubImage2D(
					GL_TEXTURE_2D,             // target
					0,                          // First mipmap level
					0,
					0,                       // x and y offset
					streamPars[deviceNr][_type].mode.getResolutionX(),
					streamPars[deviceNr][_type].mode.getResolutionY(),
					streamPars[deviceNr][_type].extFormat,
					streamPars[deviceNr][_type].pixType,
					ptr);
		}
	}
}

//---------------------------------------------------------------

bool KinectInput::uploadColorImg(int deviceNr)
{
	bool didUpdate = false;

	if (getColFrameNr(deviceNr) != lastUploadColFrame[deviceNr])
	{
		uploadImg(KI_COLOR, deviceNr);
		lastUploadColFrame[deviceNr] = getColFrameNr(deviceNr);
		didUpdate = true;
	}

	return didUpdate;
}

//---------------------------------------------------------------

bool KinectInput::uploadDepthImg(bool histo, int deviceNr)
{
	bool didUpdate = false;
	if (histo)
	{
		if (getDepthFrameNr(histo, deviceNr) != lastUploadDepthFrame[deviceNr])
		{
			uploadImg(KI_DEPTH, deviceNr, histo);
			lastUploadDepthFrame[deviceNr] = getDepthFrameNr(histo, deviceNr);
			didUpdate = true;
		}
	}
	else
	{
		if (getDepthFrameNr(histo, deviceNr) != lastUploadDepthFrameNoHisto[deviceNr])
		{
			uploadImg(KI_DEPTH, deviceNr, histo);
			lastUploadDepthFrameNoHisto[deviceNr] = getDepthFrameNr(histo, deviceNr);
			didUpdate = true;
		}
	}
	return didUpdate;
}

//---------------------------------------------------------------

bool KinectInput::uploadIrImg(int deviceNr)
{
	bool didUpdate = false;

	if (getIrFrameNr(deviceNr) != lastUploadIrFrame[deviceNr])
	{
		uploadImg(KI_IR, deviceNr);
		lastUploadIrFrame[deviceNr] = getIrFrameNr(deviceNr);
		didUpdate = true;
	}
	return didUpdate;
}

//---------------------------------------------------------------

bool KinectInput::uploadShadowImg(int deviceNr)
{
	bool didUpdate = false;

	if (nis[deviceNr] && nis[deviceNr]->isInited() && kinectShadow)
	{
		if (!shdwTexInited)
		{
			setUpdateShadow(true);

			glGenTextures(1, &shadow_tex);
			glBindTexture(GL_TEXTURE_2D, shadow_tex);

			// define inmutable storage
			glTexStorage2D(GL_TEXTURE_2D,
					1,                           // nr of mipmap levels
					GL_RGBA8,                    // internal format
					streamPars[deviceNr][KI_DEPTH].mode.getResolutionX(),
					streamPars[deviceNr][KI_DEPTH].mode.getResolutionY());

			// set linear filtering
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
			glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

			shdwTexInited = true;

		}
		else
		{
			int getThisFrame = kinectShadow->getFrameNr();

			// use internal shader
			if (kinectShadow->getFrameNr() != -1
					&& shdwUplframeNr != getThisFrame
					&& kinectShadow->getShadowImg(getThisFrame) != 0)
			{
				getThisFrame = kinectShadow->getFrameNr();

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, shadow_tex);
				glTexSubImage2D(
						GL_TEXTURE_2D,             // target
						0,                          // First mipmap level
						0,
						0,                       // x and y offset
						streamPars[deviceNr][KI_DEPTH].mode.getResolutionX(),
						streamPars[deviceNr][KI_DEPTH].mode.getResolutionY(), // width and height
						GL_BGRA,
						GL_UNSIGNED_BYTE,
						kinectShadow->getShadowImg(getThisFrame));

				shdwUplframeNr = getThisFrame;
			}
		}
		didUpdate = true;
	}
	return didUpdate;
}

//---------------------------------------------------------------

void KinectInput::setImageAutoExposure(bool onOff, int deviceNr)
{
	if (!playRecorded)
	{
		//std::cout << "deviceNr: " << deviceNr << " ptr: " << streams[deviceNr][KI_COLOR] << std::endl
		if (streams[deviceNr][KI_COLOR].getCameraSettings() == NULL)
		{
			printf(
					"setImageAutoExposure Color stream doesn't support camera settings \n");
		}
		streams[deviceNr][KI_COLOR].getCameraSettings()->setAutoExposureEnabled(
				onOff);
		if (debug)
			printf("Auto Exposure: %s\n",
					streams[deviceNr][KI_COLOR].getCameraSettings()->getAutoExposureEnabled() ?
							"ON" : "OFF");
	}
}

//---------------------------------------------------------------

void KinectInput::setImageAutoWhiteBalance(bool onOff, int deviceNr)
{
	if (!playRecorded)
	{
		if (streams[deviceNr][KI_COLOR].getCameraSettings() == NULL)
		{
			printf("Color stream doesn't support camera settings\n");
			return;
		}
		streams[deviceNr][KI_COLOR].getCameraSettings()->setAutoWhiteBalanceEnabled(
				onOff);
		if (debug)
			printf("Auto White balance: %s\n",
					streams[deviceNr][KI_COLOR].getCameraSettings()->getAutoWhiteBalanceEnabled() ?
							"ON" : "OFF");
	}
}

//---------------------------------------------------------------

void KinectInput::setImageExposure(int delta, int deviceNr)
{
	if (!playRecorded)
	{
		if (streams[deviceNr][KI_COLOR].getCameraSettings() == NULL)
		{
			printf("Color stream doesn't support camera settings\n");
			return;
		}
		int exposure =
				streams[deviceNr][KI_COLOR].getCameraSettings()->getExposure();
		openni::Status rc =
				streams[deviceNr][KI_COLOR].getCameraSettings()->setExposure(
						exposure + delta);
		if (rc != openni::STATUS_OK)
		{
			printf("Can't change exposure\n");
			return;
		}
		if (debug)
			printf("Changed exposure to: %d\n",
					streams[deviceNr][KI_COLOR].getCameraSettings()->getExposure());
	}
}

//------------------------------------------

void KinectInput::setImageRegistration(bool val, int deviceNr)
{
	if (!playRecorded)
	{
		openni::ImageRegistrationMode newMode;

		if (val)
		{
			if (debug)
				printf("set registration depth to color \n");
			newMode = openni::IMAGE_REGISTRATION_DEPTH_TO_COLOR;
		}
		else
		{
			newMode = openni::IMAGE_REGISTRATION_OFF;
		}

		if (device[deviceNr].isImageRegistrationModeSupported(newMode))
		{
			device[deviceNr].setImageRegistrationMode(newMode);
		}
		else
		{
			printf(
					"KinectInput Error: Couldn't change image registration to unsupported mode\n");
		}
	}
}

//------------------------------------------

void KinectInput::setImageGain(int delta, int deviceNr)
{
	if (!playRecorded)
	{
		if (streams[deviceNr][KI_COLOR].getCameraSettings() == NULL)
		{
			printf("%d stream doesn't support camera settings\n", KI_COLOR);
			return;
		}
		int gain = streams[deviceNr][KI_COLOR].getCameraSettings()->getGain();
		openni::Status rc =
				streams[deviceNr][KI_COLOR].getCameraSettings()->setGain(
						gain + delta);
		if (rc != openni::STATUS_OK)
		{
			printf("Can't change gain \n");
			return;
		}
		if (debug)
			printf("Changed gain to: %d \n",
					streams[deviceNr][KI_COLOR].getCameraSettings()->getGain());
	}
}

//------------------------------------------

void KinectInput::setDepthGain(int delta, int deviceNr)
{
	// kann nur gesetzt werden, wenn der stream nicht in benutzung ist
	openni::Status rc = streams[deviceNr][KI_DEPTH].setProperty(
			XN_STREAM_PROPERTY_GAIN, (uint64_t) delta);
	if (rc != openni::STATUS_OK)
		printf("Error: %s\n", openni::OpenNI::getExtendedError());
}

//------------------------------------------

void KinectInput::setCloseRange(bool _val, int deviceNr)
{
	streams[deviceNr][KI_DEPTH].setProperty(XN_STREAM_PROPERTY_CLOSE_RANGE,
			_val);

	bool bCloseRange;
	streams[deviceNr][KI_DEPTH].getProperty(XN_STREAM_PROPERTY_CLOSE_RANGE,
			&bCloseRange);

	if (debug)
		printf("Close range: %s \n", bCloseRange ? "On" : "Off");
}

//------------------------------------------

void KinectInput::setMirror(bool val, int deviceNr)
{
	if (getStream[KI_DEPTH])
		setDepthMirror(val, deviceNr);
	if (getStream[KI_COLOR])
		setColorMirror(val, deviceNr);
	if (getStream[KI_IR])
		setIRMirror(val, deviceNr);
}

//------------------------------------------

void KinectInput::setDepthMirror(bool val, int deviceNr)
{
	openni::Status stat = streams[deviceNr][KI_DEPTH].setMirroringEnabled(val);
	if (debug)
		printf("Depth Mirror: %s status: %d\n",
				streams[deviceNr][KI_DEPTH].getMirroringEnabled() ?
						"On" : "Off", stat);
}

//------------------------------------------

void KinectInput::setColorMirror(bool val, int deviceNr)
{
	openni::Status stat = streams[deviceNr][KI_COLOR].setMirroringEnabled(val);
	if (debug)
		printf("Color Mirror: %s status: %d\n",
				streams[deviceNr][KI_COLOR].getMirroringEnabled() ?
						"On" : "Off", stat);
}

//------------------------------------------

void KinectInput::setIRMirror(bool val, int deviceNr)
{
	openni::Status stat = streams[deviceNr][KI_IR].setMirroringEnabled(val);
	if (debug)
		printf("Ir Mirror: %s status: %d\n",
				streams[deviceNr][KI_IR].getMirroringEnabled() ? "On" : "Off",
				stat);
}

//------------------------------------------

void KinectInput::setDepthVMirror(bool val, int deviceNr)
{
	((KinectDepthStreamListener*) frameListeners[deviceNr][KI_DEPTH])->setVMirror(
			val);
}

//------------------------------------------

void KinectInput::setColorVMirror(bool val, int deviceNr)
{
	((KinectColorStreamListener*) frameListeners[deviceNr][KI_COLOR])->setVMirror(
			val);
}

openni::Status KinectInput::setPlaybackSpeed(float speed, int deviceNr)
{
	if (pPlaybackControl == NULL)
	{
		return openni::STATUS_NOT_SUPPORTED;
	}
	return pPlaybackControl[deviceNr]->setSpeed(speed);
}

//------------------------------------------

void KinectInput::setUpdateNis(bool _val, int deviceNr)
{
	if (nis && nis[deviceNr])
		nis[deviceNr]->doUpdate = _val;
}

//------------------------------------------

void KinectInput::setUpdateShadow(bool _val, int deviceNr)
{
	if (!kinectShadow)
	{
		while (!(nis[deviceNr] && nis[deviceNr]->getUserTrackerFrame()))
			myNanoSleep(500000);

		kinectShadow = new NIKinectShadow(
				streamPars[deviceNr][KI_DEPTH].mode.getResolutionX(),
				streamPars[deviceNr][KI_DEPTH].mode.getResolutionY(), 4, kMap,
				nis[deviceNr]->getUserTrackerFrame());
		((KinectDepthStreamListener*) frameListeners[deviceNr][KI_DEPTH])->addPostProc(
				kinectShadow);
		kinectShadow->doUpdate = _val;
		if (debug)
			printf("KinectInput::setUpdateShadow now initialized!!! \n");

	}
	else
	{
		kinectShadow->doUpdate = _val;
	}
}

//------------------------------------------

void KinectInput::setEmitter(bool _val, int deviceNr)
{
	openni::Status rc = device[deviceNr].setProperty(
			XN_MODULE_PROPERTY_EMITTER_STATE, (uint64_t) _val);
	if (rc != openni::STATUS_OK)
		printf("Error: %s\n", openni::OpenNI::getExtendedError());
}

//------------------------------------------

void KinectInput::rotIr90(int deviceNr)
{
	if (frameListeners[deviceNr][KI_IR])
		((KinectIrStreamListener*) frameListeners[deviceNr][KI_IR])->rot90();
}

//------------------------------------------

void KinectInput::rotColor90(int deviceNr)
{
	if (frameListeners[deviceNr][KI_COLOR])
		((KinectColorStreamListener*) frameListeners[deviceNr][KI_COLOR])->rot90();
}

//------------------------------------------

void KinectInput::colUseGray(int deviceNr, bool _val)
{
	((KinectColorStreamListener*) frameListeners[deviceNr][KI_COLOR])->useGray(
			_val);
}

//--------------------------------------------------------------------

int KinectInput::getNrDevices()
{
	return nrDevices;
}

//--------------------------------------------------------------------

float KinectInput::getPlaybackSpeed(int deviceNr)
{
	if (pPlaybackControl == NULL)
	{
		return 0.0f;
	}
	return pPlaybackControl[deviceNr]->getSpeed();
}

//--------------------------------------------------------------------

GLuint KinectInput::getTexId(ki_StreamType _type, int deviceNr, bool histo, int bufOffs)
{
	GLint idPtr = (streamPars[deviceNr][_type].texPtr + bufOffs + streamPars[deviceNr][_type].nrTexs)
		% streamPars[deviceNr][_type].nrTexs;

	if (histo){
		return streamPars[deviceNr][_type].texId[idPtr];
	} else {
		if (streamPars[deviceNr][_type].texIdNoHisto != 0)
			return streamPars[deviceNr][_type].texIdNoHisto[idPtr];
		else
			return 0;
	}
}

//--------------------------------------------------------------------

GLuint KinectInput::getDepthTexId(bool histo, int deviceNr, int bufOffs)
{
	return getTexId(KI_DEPTH, deviceNr, histo, bufOffs);
}

//--------------------------------------------------------------------

GLuint KinectInput::getColorTexId(int deviceNr)
{
	return getTexId(KI_COLOR, deviceNr, false);
}

//--------------------------------------------------------------------

GLuint KinectInput::getIrTexId(int deviceNr)
{
	return getTexId(KI_IR, deviceNr, false);
}

//--------------------------------------------------------------------

GLuint KinectInput::getShdwTexId(int deviceNr)
{
	int out = 0;
	if (shdwTexInited)
		out = shadow_tex;
	return out;
}

//--------------------------------------------------------------------

int KinectInput::getColorWidth(int deviceNr)
{
	return streamPars[deviceNr][KI_COLOR].mode.getResolutionX();
}

//--------------------------------------------------------------------

int KinectInput::getColorHeight(int deviceNr)
{
	return streamPars[deviceNr][KI_COLOR].mode.getResolutionY();
}

//--------------------------------------------------------------------

int KinectInput::getDepthWidth(int deviceNr)
{
	return streamPars[deviceNr][KI_DEPTH].mode.getResolutionX();
}

//--------------------------------------------------------------------

int KinectInput::getDepthHeight(int deviceNr)
{
	return streamPars[deviceNr][KI_DEPTH].mode.getResolutionY();
}

//--------------------------------------------------------------------

int KinectInput::getIrWidth(int deviceNr)
{
	return streamPars[deviceNr][KI_IR].mode.getResolutionX();
}

//--------------------------------------------------------------------

int KinectInput::getIrHeight(int deviceNr)
{
	return streamPars[deviceNr][KI_IR].mode.getResolutionY();
}

//--------------------------------------------------------------------

float KinectInput::getDepthFovY()
{
	return kPar->depthFovY;
}

//--------------------------------------------------------------------

float KinectInput::getDepthFovX()
{
	return kPar->depthFovX;
}

//--------------------------------------------------------------------

int KinectInput::getDepthMaxDepth(int deviceNr)
{
	return ((KinectDepthStreamListener*) frameListeners[deviceNr][KI_DEPTH])->getMaxDepth();
}

//--------------------------------------------------------------------

uint8_t* KinectInput::getActColorImg(int deviceNr)
{
	return ((KinectColorStreamListener*) frameListeners[deviceNr][KI_COLOR])->getActImg();
}

//--------------------------------------------------------------------

uint8_t* KinectInput::getColorImg(int deviceNr, int offset)
{
	return ((KinectColorStreamListener*) frameListeners[deviceNr][KI_COLOR])->getImg(
			offset);
}

//--------------------------------------------------------------------

uint8_t* KinectInput::getActColorGrImg(int deviceNr)
{
	return ((KinectColorStreamListener*) frameListeners[deviceNr][KI_COLOR])->getActGrayImg();
}

//--------------------------------------------------------------------

float* KinectInput::getActDepthImg(int deviceNr)
{
	return ((KinectDepthStreamListener*) frameListeners[deviceNr][KI_DEPTH])->getActImg();
}

//--------------------------------------------------------------------

float* KinectInput::getActDepthImgNoHisto(int deviceNr)
{
	return ((KinectDepthStreamListener*) frameListeners[deviceNr][KI_DEPTH])->getActImgNoHisto();
}

//--------------------------------------------------------------------

uint8_t* KinectInput::getActDepthImg8(int deviceNr)
{
	return ((KinectDepthStreamListener*) frameListeners[deviceNr][KI_DEPTH])->getActImg8();

}

//--------------------------------------------------------------------

uint8_t* KinectInput::getActIrImg(int deviceNr)
{
	return ((KinectIrStreamListener*) frameListeners[deviceNr][KI_IR])->getActImg();
}

//--------------------------------------------------------------------

uint8_t* KinectInput::getShadowImg(int deviceNr, int frameNr)
{
	uint8_t* out = nullptr;
	if (kinectShadow)
		out = kinectShadow->getShadowImg(frameNr);
	return out;
}

//--------------------------------------------------------------------

int KinectInput::getColFrameNr(int deviceNr)
{
	int out = 0;
	if (frameListeners[deviceNr][KI_COLOR])
		out = frameListeners[deviceNr][KI_COLOR]->getFrameNr();
	return out;
}

//--------------------------------------------------------------------

int KinectInput::getIrFrameNr(int deviceNr)
{
	int out = 0;
	if (frameListeners[deviceNr][KI_IR])
		out = frameListeners[deviceNr][KI_IR]->getFrameNr();
	return out;
}

//--------------------------------------------------------------------

int KinectInput::getDepthFrameNr(bool histo, int deviceNr)
{
	int out = 0;
	if (frameListeners[deviceNr][KI_DEPTH])
	{
		if (histo)
			out = frameListeners[deviceNr][KI_DEPTH]->getFrameNr();
		else
			out = frameListeners[deviceNr][KI_DEPTH]->getFrameNrNoHisto();
	}
	return out;
}

//--------------------------------------------------------------------

int KinectInput::getShadowFrameNr(int deviceNr)
{
	int out = -1;
	if (kinectShadow)
		out = kinectShadow->getFrameNr();
	return out;
}

//--------------------------------------------------------------------

int KinectInput::getNisFrameNr(int deviceNr)
{
	int out = -1;
	if (nis && nis[deviceNr])
		out = nis[deviceNr]->getFrameNr();
	return out;
}

//--------------------------------------------------------------------

int KinectInput::getColUplFrameNr(int deviceNr)
{
	return lastUploadColFrame[deviceNr];
}

//--------------------------------------------------------------------

int KinectInput::getIrUplFrameNr(int deviceNr)
{
	return lastUploadIrFrame[deviceNr];
}

//--------------------------------------------------------------------

int KinectInput::getDepthUplFrameNr(bool histo, int deviceNr)
{
	if (histo)
		return lastUploadDepthFrame[deviceNr];
	else
		return lastUploadDepthFrameNoHisto[deviceNr];
}

//--------------------------------------------------------------------

int KinectInput::getShadowUplFrameNr(int deviceNr)
{
	return lastUploadDepthFrame[deviceNr];
}

//--------------------------------------------------------------------

openni::Device* KinectInput::getDevice(int deviceNr)
{
	return &device[deviceNr];
}

//--------------------------------------------------------------------

openni::VideoFrameRef* KinectInput::getDepthFrame(int deviceNr)
{
	return frameListeners[deviceNr][KI_DEPTH]->getFrame();
}

//--------------------------------------------------------------------

openni::VideoStream* KinectInput::getDepthStream(int deviceNr)
{
	return &streams[deviceNr][KI_DEPTH];
}

//--------------------------------------------------------------------

int KinectInput::getMaxUsers()
{
	return maxUsers;
}

//--------------------------------------------------------------------

glm::vec3* KinectInput::getShadowUserCenter(int userNr, int deviceNr)
{
	glm::vec3* out = 0;
	if (kinectShadow)
		out = kinectShadow->getUserCenter(userNr);
	return out;
}

//--------------------------------------------------------------------

NISkeleton* KinectInput::getNis(int deviceNr)
{
	return nis[deviceNr];
}

//------------------------------------------

void KinectInput::pause(bool _val)
{
	if (_val)
	{
		for (short i = 0; i < nrDevices; i++)
			for (int strm = 0; strm < KI_NR_STREAMS; strm++)
				streams[i][strm].stop();
	}
	else
	{
		for (short i = 0; i < nrDevices; i++)
			for (int strm = 0; strm < KI_NR_STREAMS; strm++)
				streams[i][strm].start();
	}
}

//------------------------------------------

void KinectInput::lockDepthMutex(int deviceNr)
{
	if (frameListeners[deviceNr][KI_DEPTH])
		frameListeners[deviceNr][KI_DEPTH]->lockMutex();
}

//------------------------------------------

void KinectInput::unlockDepthMutex(int deviceNr)
{
	if (frameListeners[deviceNr][KI_DEPTH])
		frameListeners[deviceNr][KI_DEPTH]->unlockMutex();
}

//------------------------------------------

void KinectInput::onKey(GLFWwindow* window, int key, int scancode, int action,
		int mods)
{
	for (short i = 0; i < nrDevices; i++)
		if (nis[i])
			nis[i]->onKey(key, scancode, action, mods);

	if (action == GLFW_PRESS || action == GLFW_REPEAT || mods == GLFW_MOD_SHIFT)
	{
		switch (key)
		{
		case GLFW_KEY_RIGHT:
			kMap->offset->x -= 0.025f;
			std::cout << "kOffset->x " << kMap->offset->x << std::endl;
			break;
		case GLFW_KEY_LEFT:
			kMap->offset->x += 0.025f;
			std::cout << "kOffset->x " << kMap->offset->x << std::endl;
			break;
		case GLFW_KEY_UP:
			kMap->offset->y += 0.025f;
			std::cout << "kOffset->y " << kMap->offset->y << std::endl;
			break;
		case GLFW_KEY_DOWN:
			kMap->offset->y -= 0.025f;
			std::cout << "kOffset->y " << kMap->offset->y << std::endl;
			break;

		case GLFW_KEY_L:
			kMap->scale->x -= 0.025f;
			std::cout << "maxXDist " << kMap->scale->x << std::endl;
			break;
		case GLFW_KEY_J:
			kMap->scale->x += 0.025f;
			std::cout << "maxXDist " << kMap->scale->x << std::endl;
			break;
		case GLFW_KEY_I:
			kMap->scale->y += 0.025f;
			std::cout << "maxZDist " << kMap->scale->y << std::endl;
			break;
		case GLFW_KEY_K:
			kMap->scale->y -= 0.025f;
			std::cout << "maxZDist " << kMap->scale->y << std::endl;
			break;

		case GLFW_KEY_E:
			kPar->emitter = !kPar->emitter;
			std::cout << "emitter state: " << kPar->emitter << std::endl;
			setEmitter(kPar->emitter);
			break;
		}
	}
}

//------------------------------------------

void KinectInput::shutDown()
{
	bIsReady = false;

	for (short i = 0; i < nrDevices; i++)
	{
		if (nis[i])
			nis[i]->Finalize();

		for (int strm = 0; strm < KI_NR_STREAMS; strm++)
		{
			if (getStream[strm] && frameListeners[i][strm])
			{
				//frameListeners[i][strm]->getMtx()->lock();
				streams[i][strm].stop();
				streams[i][strm].destroy();
			}
		}

		//std::cout << &device[i] << std::endl;

		//if (device[i].isValid())device[i].close();
	}

	openni::OpenNI::shutdown();
}

//--------------------------------------------------------------------------------

void KinectInput::myNanoSleep(uint32_t ns)
{
	struct timespec tim;
	tim.tv_sec = 0;
	tim.tv_nsec = (long) ns;
	nanosleep(&tim, NULL);
}

//--------------------------------------------------------------------

std::vector<std::string>& KinectInput::split(const std::string &s, char delim,
		std::vector<std::string> &elems)
{
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim))
	{
		elems.push_back(item);
	}
	return elems;
}

//--------------------------------------------------------------------

std::vector<std::string> KinectInput::split(const std::string &s, char delim)
{
	std::vector<std::string> elems;
	split(s, delim, elems);
	return elems;
}

//--------------------------------------------------------------------

KinectInput::~KinectInput()
{
	delete[] nis;
}

}
