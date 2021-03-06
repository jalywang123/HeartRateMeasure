#include "FaceDetector.h"

///
/// \brief FaceDetectorHaar::FaceDetectorHaar
///
FaceDetectorHaar::FaceDetectorHaar(bool useOCL)
    :
      FaceDetectorBase(useOCL),
      m_kw(0.3),
      m_kh(0.3)
{
#if (defined WIN32 || defined _WIN32 || defined WINCE || defined __CYGWIN__)
	std::string fileName = "haarcascade_frontalface_alt2.xml";
#else
	std::string fileName = "../HeartRateMeasure/data/haarcascades/haarcascade_frontalface_alt2.xml";
#endif
    if (m_cascade.empty())
    {
		m_cascade.load(fileName);
		if (m_cascade.empty())
		{
			m_cascade.load("../" + fileName);
		}
    }
    assert(!m_cascade.empty());
}

///
/// \brief FaceDetectorHaar::~FaceDetectorHaar
///
FaceDetectorHaar::~FaceDetectorHaar()
{
}

///
/// \brief FaceDetectorHaar::DetectBiggestFace
/// \param image
/// \return
///
cv::Rect FaceDetectorHaar::DetectBiggestFace(cv::UMat image)
{
    cv::Rect res(0, 0, 0, 0);

	if (m_cascade.empty())
	{
		assert(0);
		return res;
	}

    bool findLargestObject = true;
    bool filterRects = true;

    cv::UMat im;
    if (image.channels() == 3)
    {
        cv::cvtColor(image, im, cv::COLOR_BGR2GRAY);
    }
    else
    {
        im = image;
    }

    std::vector<cv::Rect> faceRects;
    m_cascade.detectMultiScale(im,
                               faceRects,
                               1.2,
                               (filterRects || findLargestObject) ? 4 : 0,
                               findLargestObject ? cv::CASCADE_FIND_BIGGEST_OBJECT : 0,
                               cv::Size(image.cols / 4, image.rows / 4));
    if (!faceRects.empty())
    {
        res = faceRects[0];
    }

    return res;
}


///
/// \brief FaceDetectorDNN::FaceDetectorDNN
///
FaceDetectorDNN::FaceDetectorDNN(bool useOCL)
    :
      FaceDetectorBase(useOCL),
      m_confidenceThreshold(0.3)
{
#if (defined WIN32 || defined _WIN32 || defined WINCE || defined __CYGWIN__)
    m_modelConfiguration = "deploy.prototxt";
    m_modelBinary = "res10_300x300_ssd_iter_140000.caffemodel";
#else
    m_modelConfiguration = "../HeartRateMeasure/data/face_detector/deploy.prototxt";
    m_modelBinary = "../HeartRateMeasure/data/face_detector/res10_300x300_ssd_iter_140000.caffemodel";
#endif

    m_net = cv::dnn::readNetFromCaffe(m_modelConfiguration, m_modelBinary);

    assert(!m_net.empty());

    if (useOCL)
    {
        m_net.setPreferableTarget(cv::dnn::DNN_TARGET_OPENCL);
    }
}

///
/// \brief FaceDetectorDNN::~FaceDetectorDNN
///
FaceDetectorDNN::~FaceDetectorDNN()
{
}

///
/// \brief FaceDetectorDNN::DetectBiggestFace
/// \param image
/// \return
///
cv::Rect FaceDetectorDNN::DetectBiggestFace(cv::UMat image)
{
    cv::Rect res(0, 0, 0, 0);

    const size_t inWidth = 300;
    const size_t inHeight = 300;
    const double inScaleFactor = 1.0;
    const cv::Scalar meanVal(104.0, 177.0, 123.0);

    cv::Mat inputBlob = cv::dnn::blobFromImage(image, inScaleFactor, cv::Size(inWidth, inHeight), meanVal, false, false);

    m_net.setInput(inputBlob, "data");

    cv::Mat detection = m_net.forward("detection_out");

#if 0
    for (int i = 0; i < 4; ++i)
    {
        std::cout << "detection.size[" << i << "] = " << detection.size[i] << "; ";
    }
    std::cout << std::endl;
#endif

    cv::Mat detectionMat(detection.size[2], detection.size[3], CV_32F, detection.ptr<float>());

    for (int i = 0; i < detectionMat.rows; i++)
    {
        float confidence = detectionMat.at<float>(i, 2);
        int objectClass = cvRound(detectionMat.at<float>(i, 2));

        if (confidence > m_confidenceThreshold && objectClass == 1)
        {
            int xLeftBottom = cvRound(detectionMat.at<float>(i, 3) * image.cols);
            int yLeftBottom = cvRound(detectionMat.at<float>(i, 4) * image.rows);
            int xRightTop = cvRound(detectionMat.at<float>(i, 5) * image.cols);
            int yRightTop = cvRound(detectionMat.at<float>(i, 6) * image.rows);

            cv::Rect object(xLeftBottom, yLeftBottom, xRightTop - xLeftBottom, yRightTop - yLeftBottom);

            if (object.x >=0 && object.y >= 0 &&
                    object.x + object.width < image.rows &&
                    object.y + object.height < image.cols)
            {
                if (object.width > res.width)
                {
                    res = object;
                }
            }
        }

        //std::cout << "Face " << i << ": confidence = " << confidence << ", class = " << cvRound(detectionMat.at<float>(i, 2));
        //std::cout << ", rect(" << detectionMat.at<float>(i, 3) << ", " << detectionMat.at<float>(i, 4) << ", ";
        //std::cout << detectionMat.at<float>(i, 5) << ", " << detectionMat.at<float>(i, 6) << ")" << std::endl;
    }

    return res;
}

///
/// \brief FaceLandmarksDetector::FaceLandmarksDetector
///
FaceLandmarksDetector::FaceLandmarksDetector()
{
#if (defined WIN32 || defined _WIN32 || defined WINCE || defined __CYGWIN__)
    m_modelFilename = "face_landmark_model.dat";
#else
    m_modelFilename = "../HeartRateMeasure/data/face_detector/face_landmark_model.dat";
#endif

    cv::face::FacemarkKazemi::Params params;
    m_facemark = cv::face::FacemarkKazemi::create(params);
    m_facemark->loadModel(m_modelFilename);
}

///
/// \brief FaceLandmarksDetector::~FaceLandmarksDetector
///
FaceLandmarksDetector::~FaceLandmarksDetector()
{

}

///
/// \brief FaceLandmarksDetector::Detect
/// \param image
/// \param faceRect
/// \param landmarks
///
void FaceLandmarksDetector::Detect(cv::UMat image,
                                   const cv::Rect& faceRect,
                                   std::vector<cv::Point2f>& landmarks)
{
    std::vector<cv::Rect> faces = { faceRect };
    std::vector<std::vector<cv::Point2f>> shapes;

    landmarks.clear();

    if (m_facemark->fit(image, faces, shapes))
    {
        landmarks.assign(std::begin(shapes[0]), std::end(shapes[0]));
    }
}
