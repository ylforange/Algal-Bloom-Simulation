// AlgaeSalvageSim.cpp（加分项实现）
#include "AlgaeSalvageSim.h"
#include "AlgaeSimulator.h"
#include "ImageProcessor.h" 
#include <iostream>
#include <algorithm> 
#include <cmath>   


static bool is_pixel_in_circle(cv::Point p, cv::Point center, int radius_pixels) {
    return cv::norm(p - center) <= radius_pixels;
}

static std::vector<cv::Point> get_algae_pixels_in_circle(const cv::Mat& mask, cv::Point center, int radius_pixels) {
    std::vector<cv::Point> algae_pixels_in_zone;
    std::vector<cv::Point> all_algae_pixels;
    cv::findNonZero(mask, all_algae_pixels);

    for (const auto& p : all_algae_pixels) {
        if (is_pixel_in_circle(p, center, radius_pixels)) { 
            algae_pixels_in_zone.push_back(p);
        }
    }
    return algae_pixels_in_zone;
}

static void salvage_algae_pixels(cv::Mat& mask, cv::Point intake_center, int second_alert_radius_pixels, int total_pixels_to_remove_this_step) {
    std::vector<cv::Point> algae_in_second_zone;
    std::vector<cv::Point> all_algae_pixels_current;
    cv::findNonZero(mask, all_algae_pixels_current);

    for (const auto& p : all_algae_pixels_current) {
        if (is_pixel_in_circle(p, intake_center, second_alert_radius_pixels)) {
            algae_in_second_zone.push_back(p);
        }
    }

    if (algae_in_second_zone.empty()) {
        return;
    }

    std::sort(algae_in_second_zone.begin(), algae_in_second_zone.end(),
        [&](const cv::Point& a, const cv::Point& b) {
            return cv::norm(a - intake_center) < cv::norm(b - intake_center);
        });

    int num_to_remove = std::min((int)algae_in_second_zone.size(), total_pixels_to_remove_this_step);
    for (int i = 0; i < num_to_remove; ++i) {
        mask.at<uchar>(algae_in_second_zone[i].y, algae_in_second_zone[i].x) = 0;
    }
}


void runAlgaeSalvageSimulation(
    const cv::Mat& initial_algae_mask_t1,
    const cv::Mat& velocity_field_mps,
    const cv::Mat& colormap_t1,
    float spatial_resolution_meters
) {
    std::cout << "\n--- 正在启动藻华打捞模拟 ---" << std::endl;

    cv::Point taihu_intake_coord = cv::Point(758, 498);

    const float FIRST_ALERT_RADIUS_METERS = 500.0f;
    const float SECOND_ALERT_RADIUS_METERS = 1000.0f;

    const int first_alert_radius_pixels = static_cast<int>(FIRST_ALERT_RADIUS_METERS / spatial_resolution_meters);
    const int second_alert_radius_pixels = static_cast<int>(SECOND_ALERT_RADIUS_METERS / spatial_resolution_meters);

    const int PIXELS_CLEANED_PER_BOAT_PER_TIMESTEP = 30;
    const float SIM_TIME_STEP_MINUTES = 20.0f;
    const float TOTAL_SIMULATION_HOURS = 6.0f;
    const int num_sim_steps = static_cast<int>(TOTAL_SIMULATION_HOURS * 60 / SIM_TIME_STEP_MINUTES);

    AlgaeSimulator simulator;

    int min_boats_needed = 0;
    for (int num_boats = 1; ; ++num_boats) {
        std::cout << "\n--- 尝试使用 " << num_boats << " 艘打捞船进行模拟 ---" << std::endl;
        int current_health = 100;
        bool mission_failed = false;

        cv::Mat current_algae_mask = initial_algae_mask_t1.clone();

        std::vector<cv::Point> initial_algae_in_second_zone = get_algae_pixels_in_circle(current_algae_mask, taihu_intake_coord, second_alert_radius_pixels);
        for (const auto& p : initial_algae_in_second_zone) {
            current_algae_mask.at<uchar>(p.y, p.x) = 0;
        }
        std::cout << "11:13 初始清理完成。当前血条: " << current_health << std::endl;

        cv::Mat simulation_display_base = colormap_t1.clone();
        cv::circle(simulation_display_base, taihu_intake_coord, first_alert_radius_pixels, cv::Scalar(0, 0, 255), 2);
        cv::circle(simulation_display_base, taihu_intake_coord, second_alert_radius_pixels, cv::Scalar(0, 255, 255), 2);
        cv::circle(simulation_display_base, taihu_intake_coord, 3, cv::Scalar(255, 0, 0), -1);

        double sim_scale_factor = 0.7;

        for (int i = 0; i <= num_sim_steps; ++i) {
            float current_sim_minutes = i * SIM_TIME_STEP_MINUTES;

            if (i > 0) {
                cv::Mat predicted_mask_for_step = simulator.predictAlgaePosition(
                    current_algae_mask,
                    velocity_field_mps,
                    SIM_TIME_STEP_MINUTES / 60.0f,
                    spatial_resolution_meters
                );
                current_algae_mask = predicted_mask_for_step;
            }

            int total_pixels_to_clean_this_step = num_boats * PIXELS_CLEANED_PER_BOAT_PER_TIMESTEP;
            salvage_algae_pixels(current_algae_mask, taihu_intake_coord, second_alert_radius_pixels, total_pixels_to_clean_this_step);

            std::vector<cv::Point> algae_in_first_zone = get_algae_pixels_in_circle(current_algae_mask, taihu_intake_coord, first_alert_radius_pixels);
            if (!algae_in_first_zone.empty()) {
                current_health = 0;
                mission_failed = true;
                std::cout << cv::format("预警: 藻华在 11:13 + %.1f 分钟 处进入一级警戒圈，血条清零！", current_sim_minutes) << std::endl;
            }
            else {
                std::vector<cv::Point> algae_in_second_and_outside_first;
                std::vector<cv::Point> all_algae_pixels_current_after_salvage;
                cv::findNonZero(current_algae_mask, all_algae_pixels_current_after_salvage);

                for (const auto& p : all_algae_pixels_current_after_salvage) {
                    if (is_pixel_in_circle(p, taihu_intake_coord, second_alert_radius_pixels) &&
                        !is_pixel_in_circle(p, taihu_intake_coord, first_alert_radius_pixels)) {
                        algae_in_second_and_outside_first.push_back(p);
                    }
                }
                current_health -= algae_in_second_and_outside_first.size();
            }

            if (current_health < 0) {
                current_health = 0;
            }

            cv::Mat frame = createColorMapFromMask(current_algae_mask, simulation_display_base);
            cv::putText(frame, cv::format("Time: 11:13 + %.0f min", current_sim_minutes), cv::Point(30, 30),
                cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
            cv::putText(frame, cv::format("Boats: %d", num_boats), cv::Point(30, 60),
                cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(255, 255, 255), 2);
            cv::putText(frame, cv::format("Health: %d", current_health), cv::Point(30, 90),
                cv::FONT_HERSHEY_SIMPLEX, 0.8, (current_health > 20 ? cv::Scalar(0, 255, 0) : cv::Scalar(0, 0, 255)), 2);

            cv::Mat frame_display;
            cv::resize(frame, frame_display, cv::Size(), sim_scale_factor, sim_scale_factor, cv::INTER_AREA);

            cv::imshow(cv::format("Algae Salvage Simulation (Boats: %d)", num_boats), frame_display);

            if (current_health <= 0) {
                mission_failed = true;
                std::cout << cv::format("使用 %d 艘船在 11:13 + %.0f 分钟 时血条耗尽，任务失败。", num_boats, current_sim_minutes) << std::endl;
                cv::waitKey(0);
                break;
            }
            if (cv::waitKey(100) == 27) {
                mission_failed = true;
                std::cout << "用户提前退出模拟。" << std::endl;
                break;
            }
        }

        cv::destroyAllWindows();

        if (!mission_failed && current_health > 0) {
            min_boats_needed = num_boats;
            std::cout << "\n--- 任务成功！在 11:13-17:13 时间范围内，最少需要 " << min_boats_needed << " 艘打捞船保持血条大于0。---" << std::endl;
            break;
        }
        else {
            std::cout << "使用 " << num_boats << " 艘打捞船任务失败，尝试增加船只数量..." << std::endl;
        }
    }

    std::cout << "\n--- 藻华打捞模拟结束 ---" << std::endl;
}