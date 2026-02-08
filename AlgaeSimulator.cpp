// AlgaeSimulator.cpp（利用漂移矢量，驱动藻华像素漂移）

#include "AlgaeSimulator.h"
#include <vector>

AlgaeSimulator::AlgaeSimulator() {}

cv::Mat AlgaeSimulator::predictAlgaePosition(
    const cv::Mat& initial_algae_mask,
    const cv::Mat& velocity_field_mps,
    float hours_ahead,
    float spatial_resolution
) const {
    float time_in_seconds = hours_ahead * 3600.0f;
    cv::Mat displacement_field_pixels = velocity_field_mps * (time_in_seconds / spatial_resolution);

    cv::Mat predicted_mask = cv::Mat::zeros(initial_algae_mask.size(), CV_8U);

    std::vector<cv::Point> algae_locations;
    cv::findNonZero(initial_algae_mask, algae_locations);

    int rows = predicted_mask.rows;
    int cols = predicted_mask.cols;

    for (const cv::Point& start_pos : algae_locations) {
        const cv::Vec2f& displacement = displacement_field_pixels.at<cv::Vec2f>(start_pos.y, start_pos.x);

        float new_x = start_pos.x + displacement[0];
        float new_y = start_pos.y + displacement[1];

        int final_x = std::max(0, std::min(cols - 1, static_cast<int>(round(new_x))));
        int final_y = std::max(0, std::min(rows - 1, static_cast<int>(round(new_y))));

        predicted_mask.at<uchar>(final_y, final_x) = 255;
    }

    return predicted_mask;
}