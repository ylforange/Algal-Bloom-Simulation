// main.cpp
// 大作业最终版，包含所有基础项功能：
// 1. 藻华分布与速度可视化
// 2. 未来8小时动态模拟与入侵预警
// 3. 太湖镇水源地藻华打捞模拟

#include "ImageProcessor.h"
#include "AlgaeTracker.h"
#include "AlgaeSimulator.h"
#include "AlgaeSalvageSim.h"

#include <iostream>
#include <vector>
#include <string>
#include <set>
#include <limits>
#include <cctype>

// 关键地点
struct Location {
    std::string name;
    cv::Point coordinate;
};

// 创建带有白色背景的最终图像
cv::Mat createFinalImageWithWhiteBackground(const cv::Mat& content_image, const cv::Mat& data_mask) {
    cv::Mat white_canvas(content_image.size(), content_image.type(), cv::Scalar(255, 255, 255));
    content_image.copyTo(white_canvas, data_mask);
    return white_canvas;
}


// 动态模拟与入侵预警
void runOriginalDynamicSimulation(
    const cv::Mat& initial_algae_mask_t1,   
    const cv::Mat& velocity_field_mps,     
    const cv::Mat& colormap_t1,            
    float spatial_resolution_meters        
) {
    std::cout << "\n--- 正在启动藻华入侵动态模拟 (未来8小时) ---" << std::endl;

    const int WARNING_RADIUS = 30;
    std::vector<Location> water_intakes = {
        {"沙渚水源地", cv::Point(655, 334)},
        {"太湖镇水源地", cv::Point(758, 498)},
        {"渔洋山水源地", cv::Point(875, 741)}
    };
    std::vector<Location> scenic_spots = {
        {"七里风光堤", cv::Point(361, 350)},
        {"静山夕阳观景处", cv::Point(651, 919)},
        {"香山景区", cv::Point(77, 878)},
        {"太湖旅游度假区", cv::Point(390, 1290)}
    };

    AlgaeSimulator simulator;
    std::set<std::string> triggered_warnings;

    cv::Mat simulation_background = colormap_t1.clone();
    for (const auto& loc : water_intakes) {
        cv::circle(simulation_background, loc.coordinate, WARNING_RADIUS, cv::Scalar(0, 0, 255), 2); // 水源地用红色
    }
    for (const auto& loc : scenic_spots) {
        cv::circle(simulation_background, loc.coordinate, WARNING_RADIUS, cv::Scalar(0, 255, 255), 2); // 景区用黄色
    }

    const float SIMULATION_HOURS = 8.0f;
    const float TIME_STEP_MINUTES = 20.0f;
    const int num_steps = static_cast<int>(SIMULATION_HOURS * 60 / TIME_STEP_MINUTES);

    double sim_scale_factor = 0.7;

    for (int i = 0; i <= num_steps; ++i) {
        float current_hours = i * TIME_STEP_MINUTES / 60.0f;

        cv::Mat predicted_mask = simulator.predictAlgaePosition(
            initial_algae_mask_t1, 
            velocity_field_mps,
            current_hours,         
            spatial_resolution_meters
        );

        cv::Mat frame = createColorMapFromMask(predicted_mask, simulation_background);
        cv::putText(frame, cv::format("Time: +%.1f hours", current_hours), cv::Point(30, 30),
            cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(255, 255, 255), 2);

        auto check_warnings = [&](const std::vector<Location>& locations) {
            for (const auto& loc : locations) {
                if (triggered_warnings.count(loc.name)) continue;

                if (loc.coordinate.y >= 0 && loc.coordinate.y < predicted_mask.rows &&
                    loc.coordinate.x >= 0 && loc.coordinate.x < predicted_mask.cols) {
                    if (predicted_mask.at<uchar>(loc.coordinate.y, loc.coordinate.x) == 255) {
                        std::cout << "[!] 预警: " << cv::format("藻华预计在 %.1f 小时后", current_hours)
                            << "到达 [" << loc.name << "]！坐标: (" << loc.coordinate.x << ", " << loc.coordinate.y << ")" << std::endl;
                        triggered_warnings.insert(loc.name);
                    }
                }
            }
            };

        check_warnings(water_intakes);
        check_warnings(scenic_spots);

        cv::Mat frame_display;
        cv::resize(frame, frame_display, cv::Size(), sim_scale_factor, sim_scale_factor, cv::INTER_AREA);

        cv::imshow("Dynamic Simulation (Press ESC to exit)", frame_display);
        if (cv::waitKey(100) == 27) {
            break;
        }
    }

    std::cout << "\n--- 原始动态模拟结束 ---" << std::endl;
    cv::waitKey(0);
}


int main() {
    system("chcp 65001 > nul");
    setlocale(LC_ALL, "zh-CN.UTF-8");

    // --- 第1阶段：加载数据与核心计算 ---
    std::string path_t0 = "data/2021_05_30_10_38_06_GF1.tif";
    std::string path_t1 = "data/2021_05_30_11_13_47_GF4.tif";

    ImageProcessor processor_t0(path_t0);
    ImageProcessor processor_t1(path_t1);
    if (!processor_t0.isLoaded() || !processor_t1.isLoaded()) {
        std::cerr << "错误：图像加载失败，请检查路径和文件。" << std::endl;
        return -1;
    }

    cv::Mat ndvi_t0 = processor_t0.calculateNDVI();
    cv::Mat ndvi_t1 = processor_t1.calculateNDVI();
    cv::Mat mask_t1 = processor_t1.extractAlgaeMask(ndvi_t1); 
    if (ndvi_t0.empty() || ndvi_t1.empty() || mask_t1.empty()) {
        std::cerr << "错误：NDVI 或藻华掩膜计算失败。" << std::endl;
        return -1;
    }

    AlgaeTracker tracker;
    cv::Mat raw_flow = tracker.calculateOpticalFlow(ndvi_t0, ndvi_t1);
    cv::Mat filtered_flow = tracker.filterFlowByMask(raw_flow, mask_t1);

    const float TIME_INTERVAL_SECONDS = 2141.0f;
    const float SPATIAL_RESOLUTION_METERS = 50.0f;
    cv::Mat velocity_field_mps = filtered_flow * (SPATIAL_RESOLUTION_METERS / TIME_INTERVAL_SECONDS);

    // --- 第2阶段：生成静态的可视化成果图 ---
    std::cout << "--- 正在生成静态分析图 ---" << std::endl;
    cv::Mat data_mask = processor_t0.createDataMask();
    cv::Mat colormap_t0 = processor_t0.createNDVIColorMap(ndvi_t0);
    cv::Mat colormap_t1 = processor_t1.createNDVIColorMap(ndvi_t1);

    cv::Mat final_t0 = createFinalImageWithWhiteBackground(colormap_t0, data_mask);
    cv::Mat final_t1 = createFinalImageWithWhiteBackground(colormap_t1, data_mask);

    cv::Mat stage1_result;
    cv::hconcat(final_t0, final_t1, stage1_result);

    cv::Mat flow_viz = tracker.visualizeFlow(colormap_t0, filtered_flow, 25);
    cv::arrowedLine(flow_viz, cv::Point(50, flow_viz.rows - 80), cv::Point(150, flow_viz.rows - 80), cv::Scalar(0, 0, 255), 2, cv::LINE_AA);
    cv::putText(flow_viz, "Drift Velocity", cv::Point(50, flow_viz.rows - 55), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
    cv::putText(flow_viz, "0.3 m/s", cv::Point(50, flow_viz.rows - 35), cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
    cv::Mat stage2_result = createFinalImageWithWhiteBackground(flow_viz, data_mask);

    double static_scale_factor = 0.5;
    cv::Mat stage1_display, stage2_display;
    cv::resize(stage1_result, stage1_display, cv::Size(), static_scale_factor, static_scale_factor, cv::INTER_AREA);
    cv::resize(stage2_result, stage2_display, cv::Size(), static_scale_factor, static_scale_factor, cv::INTER_AREA);

    cv::imshow("Stage 1 - Algae Distribution (Scaled)", stage1_display);
    cv::imshow("Stage 2 - Algae Drift Velocity (Scaled)", stage2_display);
    std::cout << "已生成分析图。正在运行动态模拟" << std::endl;
    runOriginalDynamicSimulation(mask_t1, velocity_field_mps, colormap_t1, SPATIAL_RESOLUTION_METERS);

    // 加分项
    char choice;
    std::cout << "\n是否要运行藻华打捞模拟 (加分项)? (Y/N): ";
    std::cin >> choice;
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    if (toupper(choice) == 'Y') {
        runAlgaeSalvageSimulation(mask_t1, velocity_field_mps, colormap_t1, SPATIAL_RESOLUTION_METERS);
    }
    std::cout << "\n所有模拟任务结束。" << std::endl;
    return 0;
}