#include "opencv-helpers.hpp"

PatternImageData createPatternData(QImage &pattern)
{
	PatternImageData data{};
	if (pattern.isNull()) {
		return data;
	}

	data.rgbaPattern = QImageToMat(pattern);
	std::vector<cv::Mat1b> rgbaChannelsPattern;
	cv::split(data.rgbaPattern, rgbaChannelsPattern);
	std::vector<cv::Mat1b> rgbChanlesPattern(
		rgbaChannelsPattern.begin(), rgbaChannelsPattern.begin() + 3);
	cv::merge(rgbChanlesPattern, data.rgbPattern);
	cv::threshold(rgbaChannelsPattern[3], data.mask, 0, 255,
		      cv::THRESH_BINARY);
	return data;
}

void matchPattern(QImage &img, const PatternImageData &patternData,
		  double threshold, cv::Mat &result, bool useAlphaAsMask)
{
	if (img.isNull() || patternData.rgbaPattern.empty()) {
		return;
	}
	if (img.height() < patternData.rgbaPattern.rows ||
	    img.width() < patternData.rgbaPattern.cols) {
		return;
	}

	auto i = QImageToMat(img);

	if (useAlphaAsMask) {
		std::vector<cv::Mat1b> rgbaChannelsImage;
		cv::split(i, rgbaChannelsImage);
		std::vector<cv::Mat1b> rgbChanlesImage(
			rgbaChannelsImage.begin(),
			rgbaChannelsImage.begin() + 3);

		cv::Mat3b rgbImage;
		cv::merge(rgbChanlesImage, rgbImage);

		cv::matchTemplate(rgbImage, patternData.rgbPattern, result,
				  cv::TM_CCORR_NORMED, patternData.mask);
		cv::threshold(result, result, threshold, 0, cv::THRESH_TOZERO);

	} else {
		cv::matchTemplate(i, patternData.rgbaPattern, result,
				  cv::TM_CCOEFF_NORMED);
		cv::threshold(result, result, threshold, 0, cv::THRESH_TOZERO);
	}
}

void matchPattern(QImage &img, QImage &pattern, double threshold,
		  cv::Mat &result, bool useAlphaAsMask)
{
	auto data = createPatternData(pattern);
	matchPattern(img, data, threshold, result, useAlphaAsMask);
}

std::vector<cv::Rect> matchObject(QImage &img, cv::CascadeClassifier &cascade,
				  double scaleFactor, int minNeighbors,
				  cv::Size minSize, cv::Size maxSize)
{
	if (img.isNull() || cascade.empty()) {
		return {};
	}

	auto i = QImageToMat(img);
	cv::Mat frameGray;
	cv::cvtColor(i, frameGray, cv::COLOR_RGBA2GRAY);
	equalizeHist(frameGray, frameGray);
	std::vector<cv::Rect> objects;
	cascade.detectMultiScale(frameGray, objects, scaleFactor, minNeighbors,
				 0, minSize, maxSize);
	return objects;
}

uchar getAvgBrightness(QImage &img)
{
	auto i = QImageToMat(img);
	cv::Mat hsvImage, rgbImage;
	cv::cvtColor(i, rgbImage, cv::COLOR_RGBA2RGB);
	cv::cvtColor(rgbImage, hsvImage, cv::COLOR_RGB2HSV);
	long long brightnessSum = 0;
	for (int i = 0; i < hsvImage.rows; ++i) {
		for (int j = 0; j < hsvImage.cols; ++j) {
			brightnessSum += hsvImage.at<cv::Vec3b>(i, j)[2];
		}
	}
	return brightnessSum / (hsvImage.rows * hsvImage.cols);
}

cv::Mat preprocessForOCR(const QImage &image, const QColor &color)
{
	auto mat = QImageToMat(image);

	// Only keep the desired color
	cv::cvtColor(mat, mat, cv::COLOR_RGBA2RGB);
	cv::cvtColor(mat, mat, cv::COLOR_RGB2HSV);
	cv::inRange(mat, cv::Scalar(0, 0, 0),
		    cv::Scalar(color.red(), color.green(), color.blue()), mat);

	// Invert to improve ORC detection
	cv::bitwise_not(mat, mat);

	// Scale image up if selected area is too small
	// Results will probably still be unsatisfying
	if (mat.rows <= 300 || mat.cols <= 300) {
		double scale = 0.;
		if (mat.rows < mat.cols) {
			scale = 300. / mat.rows;
		} else {
			scale = 300. / mat.cols;
		}

		cv::resize(mat, mat,
			   cv::Size(mat.cols * scale, mat.rows * scale),
			   cv::INTER_CUBIC);
	}

	return mat;
}

std::string runOCR(tesseract::TessBaseAPI *ocr, const QImage &image,
		   const QColor &color)
{
#ifdef OCR_SUPPORT
	auto mat = preprocessForOCR(image, color);
	ocr->SetImage(mat.data, mat.cols, mat.rows, 1, mat.step);
	ocr->Recognize(0);
	std::unique_ptr<char[]> detectedText(ocr->GetUTF8Text());

	if (!detectedText) {
		return "";
	}
	return detectedText.get();

#else
	return "";
#endif
}

// Assumption is that QImage uses Format_RGBA8888.
// Conversion from: https://github.com/dbzhang800/QtOpenCV
cv::Mat QImageToMat(const QImage &img)
{
	if (img.isNull()) {
		return cv::Mat();
	}
	return cv::Mat(img.height(), img.width(), CV_8UC(img.depth() / 8),
		       (uchar *)img.bits(), img.bytesPerLine());
}

QImage MatToQImage(const cv::Mat &mat)
{
	if (mat.empty()) {
		return QImage();
	}
	return QImage(mat.data, mat.cols, mat.rows,
		      QImage::Format::Format_RGBA8888);
}