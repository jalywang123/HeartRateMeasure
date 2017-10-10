#include "SignalProcessor.h"
#include "FastICA.h"
#include "pca.h"

///
/// \brief SignalProcessor::SignalProcessor
/// \param framesCount
///
SignalProcessor::SignalProcessor(size_t framesCount, RGBFilters filterType)
    :
      m_size(framesCount),
      m_filterType(filterType),
      m_minFreq(0),
      m_maxFreq(0),
      m_currFreq(0)
{
}

///
/// \brief SignalProcessor::Reset
///
void SignalProcessor::Reset()
{
    m_queue.clear();
    m_FF = freq_t();
    m_minFreq = 0;
    m_maxFreq = 0;
    m_currFreq = 0;
}

///
/// \brief SignalProcessor::AddMeasure
/// \param captureTime
/// \param val
///
void SignalProcessor::AddMeasure(TimerTimestamp captureTime, cv::Vec3d val)
{
    m_queue.push_back(Measure(captureTime, val));
    if (m_queue.size() > m_size)
    {
        m_queue.pop_front();
    }
}

///
/// \brief SignalProcessor::GetFreq
/// \return
///
double SignalProcessor::GetFreq() const
{
    return m_FF.CurrValue();
}

///
/// \brief SignalProcessor::GetInstantaneousFreq
/// \param minFreq
/// \param maxFreq
/// \return
///
double SignalProcessor::GetInstantaneousFreq(
        double* minFreq,
        double* maxFreq
        ) const
{
    if (minFreq)
    {
        *minFreq = m_minFreq;
    }
    if (maxFreq)
    {
        *maxFreq = m_maxFreq;
    }
    return m_currFreq;
}

///
/// \brief SignalProcessor::FindValueForTime
/// \param _t
/// \return
///
cv::Vec3d SignalProcessor::FindValueForTime(TimerTimestamp _t)
{
    if (m_queue.empty())
    {
        return 0;
    }

    auto it_prev = m_queue.begin();
    for (auto it = m_queue.begin(); it < m_queue.end(); ++it)
    {
        Measure m = *it;
        if (m.t >= _t)
        {
            if (it_prev->t == it->t)
            {
                return it->val;
            }
            else
            {
                double dt = double(it->t - it_prev->t);
                cv::Vec3d d_val = cv::Vec3d(it->val - it_prev->val);
                double t_rel = _t - it_prev->t;
                cv::Vec3d val_rel = d_val * (t_rel / dt);
                return val_rel + it_prev->val;
            }
        }
        it_prev = it;
    }
    assert(0);
    return cv::Vec3d();
}

///
/// \brief SignalProcessor::UniformTimedPoints
/// \param NumSamples
/// \param dst
/// \param dt
/// \param Freq
///
void SignalProcessor::UniformTimedPoints(int NumSamples, cv::Mat& dst, double& dt, double Freq)
{
    if (dst.empty() ||
            dst.size() != cv::Size(3, NumSamples))
    {
        dst = cv::Mat(3, NumSamples, CV_64FC1);
    }

    dt = (m_queue.back().t - m_queue.front().t) / (double)NumSamples;
    TimerTimestamp T = m_queue.front().t;
    for (int i = 0; i < NumSamples; ++i)
    {
        T += dt;

        cv::Vec3d val = FindValueForTime(T);
        dst.at<double>(0, i) = val[0];
        dst.at<double>(1, i) = val[1];
        dst.at<double>(2, i) = val[2];
    }
    dt /= Freq;
}

///
/// \brief SignalProcessor::FilterRGBSignal
/// \param src
/// \param dst
/// \param filterType
///
void SignalProcessor::FilterRGBSignal(cv::Mat& src, cv::Mat& dst)
{
    switch (m_filterType)
    {
    case FilterICA:
    {
        cv::Mat W;
        cv::Mat d;
        int N = 0; // Номер независимой компоненты, используемой для измерения частоты
        FastICA fica;
        fica.apply(src, d, W); // Производим разделение компонентов
        d.row(N) *= (W.at<double>(N, N) > 0) ? 1 : -1; // Инверсия при отрицательном коэффициенте
        dst = d.row(N).clone();
    }
        break;

    case FilterPCA:
        MakePCA(src, dst);
        break;
    }
    cv::normalize(dst, dst, 0, 1, cv::NORM_MINMAX);
}

///
/// \brief SignalProcessor::FilterRGBSignal
/// \param src
/// \param dst
/// \param filterType
///
void SignalProcessor::FilterRGBSignal(cv::Mat& src, std::vector<cv::Mat>& dst)
{
    switch (m_filterType)
    {
    case FilterICA:
    {
        cv::Mat W;
        cv::Mat d;
        FastICA fica;
        fica.apply(src, d, W); // Производим разделение компонентов

        dst.resize(d.rows);
        for (int i = 0; i < d.rows; ++i)
        {
            d.row(i) *= (W.at<double>(i, i) > 0) ? 1 : -1; // Инверсия при отрицательном коэффициенте
            dst[i] = d.row(i).clone();
            cv::normalize(dst[i], dst[i], 0, 1, cv::NORM_MINMAX);
        }
    }
        break;

    case FilterPCA:
        break;
    }
}

///
/// \brief SignalProcessor::MakeFourier
/// \param signal
/// \param deltaTime
/// \param currFreq
/// \param minFreq
/// \param maxFreq
/// \param draw
/// \param img
///
void SignalProcessor::MakeFourier(
        cv::Mat& signal,
        double deltaTime,
        double& currFreq,
        double& minFreq,
        double& maxFreq,
        bool draw,
        cv::Mat img
        )
{
    // Преобразование Фурье
    cv::Mat res = signal.clone();
    cv::Mat z = cv::Mat::zeros(1, signal.cols, CV_64FC1);
    std::vector<cv::Mat> ch;
    ch.push_back(res);
    ch.push_back(z);
    cv::merge(ch, res);

    cv::Mat res_freq;
    cv::dft(res, res_freq);
    cv::split(res_freq, ch);
    // Мощность спектра
    cv::magnitude(ch[0], ch[1], signal);
    // Квадрат мощности спектра
    cv::pow(signal, 2.0, signal);

    // Теперь частотный фильтр :)
    cv::line(signal, cv::Point(0, 0), cv::Point(15, 0), cv::Scalar::all(0), 1, CV_AA);
    cv::line(signal, cv::Point(100, 0), cv::Point(signal.cols - 1, 0), cv::Scalar::all(0), 1, CV_AA);

    // Чтобы все разместилось
    cv::normalize(signal, signal, 0, 1, cv::NORM_MINMAX);

    // Найдем 2 пика на частотном разложении
    const size_t INDS_COUNT = 2;
    int inds[INDS_COUNT] = { -1 };
    std::deque<double> maxVals;
    maxVals.push_back(signal.at<double>(0, 0));
    for (int x = 1; x < signal.cols; ++x)
    {
        double val = signal.at<double>(0, x);
        int ind = x;
        for (size_t i = 0; i < maxVals.size(); ++i)
        {
            if (maxVals[i] < val)
            {
                std::swap(maxVals[i], val);
                std::swap(inds[i], ind);
            }
        }
        if (maxVals.size() < INDS_COUNT)
        {
            maxVals.push_back(val);
            inds[maxVals.size() - 1] = ind;
        }
    }

    // И вычислим частоту
    maxFreq = 60.0 / (1 * deltaTime);
    minFreq = 60.0 / ((signal.cols - 1) * deltaTime);

    currFreq = -1;
    for (size_t i = 0; i < maxVals.size(); ++i)
    {
        if (inds[i] > 0)
        {
            double freq = 60.0 / (inds[i] * deltaTime);
            m_FF.AddMeasure(freq);

            if (currFreq < 0)
            {
                currFreq = freq;
                std::cout << "signal.size = " << signal.cols << ", maxInd = " << inds[i] << ", deltaTime = " << deltaTime << ", freq [" << minFreq << ", " << maxFreq << "] = " << currFreq << " - " << m_FF.CurrValue() << std::endl;
            }
        }
    }
    if (currFreq < 0)
    {
        currFreq = 0;
    }
    if (draw)
    {
        double scale_x = (double)img.cols / (double)m_queue.size();

        // Изобразим спектр Фурье
        float S = 50;
        for (int x = 1; x < signal.cols; ++x)
        {
            bool findInd = false;

            for (auto i : inds)
            {
                if (i == x)
                {
                    findInd = true;
                    break;
                }
            }

            cv::line(img,
                     cv::Point(scale_x * x, img.rows - S * signal.at<double>(x)),
                     cv::Point(scale_x * x, img.rows),
                     findInd ? cv::Scalar(255, 0, 255) : cv::Scalar(255, 255, 255));
        }

        std::vector<double> robustFreqs;
        m_FF.RobustValues(robustFreqs);

        std::cout << "Robust frequences: ";
        for (auto v : robustFreqs)
        {
            std::cout << v << " ";
        }
        std::cout << std::endl;
    }
}

///
/// \brief SignalProcessor::MeasureFrequency
/// \param img
/// \param Freq
///
void SignalProcessor::MeasureFrequency(cv::Mat& img, double Freq)
{
    if (m_queue.size() < m_size / 2)
    {
        return;
    }
    img.setTo(0);

    cv::Mat src;

    // Чтобы частота сэмплирования не плавала,
    // разместим сигнал с временными метками на равномерной сетке.

    double dt;
    UniformTimedPoints(static_cast<int>(m_queue.size()), src, dt, Freq);

    switch (m_filterType)
    {
    case FilterPCA:
    {
        // Разделяем сигналы
        cv::Mat dst;
        FilterRGBSignal(src, dst);

        DrawSignal(std::vector<cv::Mat>({ dst }), dt);

        MakeFourier(dst, dt, m_currFreq, m_minFreq, m_maxFreq, true, img);
    }
        break;

    case FilterICA:
    {
        // Разделяем сигналы
        std::vector<cv::Mat> dstArr;
        FilterRGBSignal(src, dstArr);

        DrawSignal(dstArr, dt);

        for (size_t di = 0; di < dstArr.size(); ++di)
        {
            auto& dst = dstArr[di];

            double currFreq = 0;
            double minFreq = 0;
            double maxFreq = 0;
            MakeFourier(dst, dt, currFreq, minFreq, maxFreq, di == 0, img);

            if (di == 0)
            {
                m_currFreq = currFreq;
                m_maxFreq = maxFreq;
                m_minFreq = minFreq;
            }
        }
    }
        break;
    }
    m_FF.Visualize();
}

///
/// \brief SignalProcessor::DrawSignal
/// \param signal
/// \param deltaTime
///
void SignalProcessor::DrawSignal(const std::vector<cv::Mat>& signal, double deltaTime)
{
    const int wndHeight = 200;
    cv::Mat img(signal.size() * wndHeight, 512, CV_8UC3, cv::Scalar::all(255));

    for (size_t si = 0; si < signal.size(); ++si)
    {
        cv::Mat snorm;
        cv::normalize(signal[si], snorm, wndHeight, 0, cv::NORM_MINMAX);

        double timeSum = 0;
        double v0 = snorm.at<double>(0, 0);
        for (int i = 1; i < snorm.cols; ++i)
        {
            double v1 = snorm.at<double>(0, i);

            cv::Point pt0(((i - 1) * img.cols) / snorm.cols, (si + 1) * wndHeight - v0);
            cv::Point pt1((i * img.cols) / snorm.cols, (si + 1) * wndHeight - v1);

            cv::line(img, pt0, pt1, cv::Scalar(0, 0, 0));

            int dtPrev = static_cast<int>(1000. * timeSum) / 1000;
            timeSum += deltaTime;
            int dtCurr = static_cast<int>(1000. * timeSum) / 1000;
            if (dtCurr > dtPrev)
            {
                cv::line(img, cv::Point(pt1.x, si * wndHeight), cv::Point(pt1.x, (si + 1) * wndHeight - 1), cv::Scalar(0, 150, 0));
            }

            v0 = v1;
        }
        cv::line(img, cv::Point(0, (si + 1) * wndHeight), cv::Point(img.cols - 1, (si + 1) * wndHeight), cv::Scalar(0, 0, 0));
    }

    cv::namedWindow("signal", cv::WINDOW_AUTOSIZE);
    cv::imshow("signal", img);
}
