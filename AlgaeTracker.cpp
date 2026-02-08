// AlgaeTracker.cpp（提取藻华漂移矢量）

#include "AlgaeTracker.h"
#include <vector>

AlgaeTracker::AlgaeTracker() {}

cv::Mat formatImageForFlow(const cv::Mat& ndvi_image) {
    cv::Mat formatted_image;
    ndvi_image.copyTo(formatted_image);


    cv::threshold(formatted_image, formatted_image, 0.6, 0.6, cv::THRESH_TRUNC);
    cv::threshold(formatted_image, formatted_image, -0.2, -0.2, cv::THRESH_TOZERO);

    formatted_image = (formatted_image + 0.2) / 0.8 * 255.0;

    cv::Mat final_image_8u;
    formatted_image.convertTo(final_image_8u, CV_8U);

    return final_image_8u;
}


cv::Mat AlgaeTracker::calculateOpticalFlow(const cv::Mat& ndvi_t0, const cv::Mat& ndvi_t1) {
    if (ndvi_t0.empty() || ndvi_t1.empty()) {
        return cv::Mat();
    }

    cv::Mat prev = formatImageForFlow(ndvi_t0);
    cv::Mat curr = formatImageForFlow(ndvi_t1);

    cv::Mat flow;
    cv::calcOpticalFlowFarneback(prev, curr, flow, 0.5, 5, 80, 10, 7, 1.5, cv::OPTFLOW_FARNEBACK_GAUSSIAN);

    std::vector<cv::Mat> flow_channels;
    cv::split(flow, flow_channels);
    flow_channels[1] = -flow_channels[1];
    cv::merge(flow_channels, flow);

    return flow;
}

cv::Mat AlgaeTracker::filterFlowByMask(const cv::Mat& flow_field, const cv::Mat& algae_mask) {
    if (flow_field.empty() || algae_mask.empty()) {
        return cv::Mat();
    }

    cv::Mat filtered_flow = cv::Mat::zeros(flow_field.size(), flow_field.type());
    flow_field.copyTo(filtered_flow, algae_mask);

    return filtered_flow;
}

cv::Vec2f AlgaeTracker::calculateAverageDrift(const cv::Mat& filtered_flow, const cv::Mat& algae_mask) {
    if (filtered_flow.empty() || algae_mask.empty()) {
        return cv::Vec2f(0, 0);
    }
    cv::Scalar mean_flow = cv::mean(filtered_flow, algae_mask);

    return cv::Vec2f(mean_flow[0], mean_flow[1]);
}


cv::Mat AlgaeTracker::visualizeFlow(const cv::Mat& image_to_draw_on, const cv::Mat& flow_to_visualize, int step) {
    cv::Mat display_image;
    if (image_to_draw_on.channels() == 1) {
        cv::cvtColor(image_to_draw_on, display_image, cv::COLOR_GRAY2BGR);
    }
    else {
        image_to_draw_on.copyTo(display_image);
    }

    for (int y = 0; y < display_image.rows; y += step) {
        for (int x = 0; x < display_image.cols; x += step) {
            const cv::Point2f& flow_at_point = flow_to_visualize.at<cv::Point2f>(y, x);

            if (cv::norm(flow_at_point) < 0.1) continue;

            cv::Point start_point(x, y);
            cv::Point end_point(cvRound(x + flow_at_point.x * 10), cvRound(y + flow_at_point.y * 10));

            cv::arrowedLine(display_image, start_point, end_point, cv::Scalar(0, 0, 255), 1, cv::LINE_AA);
        }
    }
    return display_image;
}