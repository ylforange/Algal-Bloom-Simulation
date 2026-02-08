// AlgaeSalvageSim.h
#ifndef ALGAE_SALVAGE_SIM_H
#define ALGAE_SALVAGE_SIM_H

#include <opencv2/opencv.hpp> 
#include <string>
#include <vector>

class AlgaeSimulator;

void runAlgaeSalvageSimulation(
    const cv::Mat& initial_algae_mask_t1,
    const cv::Mat& velocity_field_mps,   
    const cv::Mat& colormap_t1,           
    float spatial_resolution_meters      
);

#endif 