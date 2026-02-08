// AlgaeTracker.h

#pragma once

#include <opencv2/opencv.hpp>

class AlgaeTracker {
public:
    AlgaeTracker();
    cv::Mat calculateOpticalFlow(const cv::Mat& ndvi_t0, const cv::Mat& ndvi_t1);
    cv::Mat filterFlowByMask(const cv::Mat& flow_field, const cv::Mat& algae_mask);
    cv::Vec2f calculateAverageDrift(const cv::Mat& filtered_flow, const cv::Mat& algae_mask);
    cv::Mat visualizeFlow(const cv::Mat& image_to_draw_on, const cv::Mat& flow_to_visualize, int step = 30);
};