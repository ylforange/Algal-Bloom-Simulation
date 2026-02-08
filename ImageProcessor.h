// ImageProcessor.h
#ifndef IMAGE_PROCESSOR_H
#define IMAGE_PROCESSOR_H

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

class ImageProcessor {
public:
    ImageProcessor(const std::string& image_path);
    bool isLoaded() const;
    cv::Mat calculateNDVI();
    cv::Mat extractAlgaeMask(const cv::Mat& ndvi_image);
    std::vector<cv::Mat> getBands() const;
    cv::Mat createNDVIColorMap(const cv::Mat& ndvi_image);
    cv::Mat createDataMask();

private:
    cv::Mat image_data_;
    bool loaded_successfully_;
};
cv::Mat createColorMapFromMask(const cv::Mat& mask, const cv::Mat& background_template);


#endif