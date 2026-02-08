// AlgaeSimulator.h

#pragma once

#include <opencv2/opencv.hpp>

class AlgaeSimulator {
public:
    AlgaeSimulator();
    cv::Mat predictAlgaePosition(
        const cv::Mat& initial_algae_mask,
        const cv::Mat& velocity_field_mps,
        float hours_ahead,
        float spatial_resolution = 50.0f
    ) const;
};