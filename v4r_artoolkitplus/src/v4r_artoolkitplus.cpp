/***************************************************************************
 *   Copyright (C) 2013 by Markus Bader                                    *
 *   markus.bader@tuwien.ac.at                                             *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <v4r_artoolkitplus/v4r_artoolkitplus.h>
#include <v4r_artoolkitplus/v4r_artoolkitplus_defaults.h>

#include <v4r_msgs/TransformArrayStamped.h>

#include <opencv/highgui.h>
#include <cv_bridge/cv_bridge.h>
#include <sensor_msgs/image_encodings.h>

#include "ARToolKitPlus/TrackerSingleMarkerImpl.h"
#include "ARToolKitPlus/TrackerMultiMarkerImpl.h"
#include <ARToolKitPlus/CameraAdvImpl.h>



class MyLogger: public ARToolKitPlus::Logger {
    void artLog(const char* nStr) {
        printf("%s", nStr);
    }
};

ARToolKitPlusNode::ARToolKitPlusNode(ros::NodeHandle & n) :
    n_(n), n_param_("~"), callback_counter_(0), imageTransport_(n_),  logger_(NULL), param_() {

    init();
    cameraSubscriber_ = imageTransport_.subscribeCamera( ARTOOLKITPLUS_IMAGE_SRC, 1, &ARToolKitPlusNode::imageCallback, this);
}


ARToolKitPlusNode::~ARToolKitPlusNode() {
    if (logger_ != NULL)
        delete logger_;
}

class ARCamera: public ARToolKitPlus::CameraAdvImpl {
public:
    virtual ~ARCamera() {
    };

    bool isInputDistorted() {
        if (undist_iterations == 1) {
            return false;
        } else {
            return true;
        }
    }

    ARCamera(const sensor_msgs::CameraInfoConstPtr& _camer_info, int _undist_iterations, bool _input_distorted) {


        // ARToolKitPlus::CameraAdvImpl Parameter
        if (_input_distorted) {
            // using the ros Intrinsic camera matrix for the raw (distorted) images.
            this->fc[0] = (ARFloat) _camer_info->K[0];
            this->fc[1] = (ARFloat) _camer_info->K[4];
            this->cc[0] = (ARFloat) _camer_info->K[2];
            this->cc[1] = (ARFloat) _camer_info->K[5];

            undist_iterations = _undist_iterations;

            this->kc[0] = (ARFloat) _camer_info->D[0];
            this->kc[1] = (ARFloat) _camer_info->D[1];
            this->kc[2] = (ARFloat) _camer_info->D[2];
            this->kc[3] = (ARFloat) _camer_info->D[3];
            this->kc[4] = (ARFloat) _camer_info->D[4];
            this->kc[5] = (ARFloat) 0.;

        } else {
            // using the ros Projection/camera matrix
            this->fc[0] = (ARFloat) _camer_info->P[0];
            this->fc[1] = (ARFloat) _camer_info->P[5];
            this->cc[0] = (ARFloat) _camer_info->P[2];
            this->cc[1] = (ARFloat) _camer_info->P[6];

            undist_iterations = 1;

            for (int i = 0; i < 6; i++)
                this->kc[i] = (ARFloat) 0.;

        }

        // ARToolKitPlus::Camera Parameter
        // fileName

        // ARToolKit::ARParam Parameter
        xsize = _camer_info->width;
        ysize = _camer_info->height;

        for (int i = 0; i < 3; i++)
            for (int j = 0; j < 4; j++)
                this->mat[i][j] = (ARFloat) 0.;

        mat[0][0] = fc[0]; // fc_x
        mat[1][1] = fc[1]; // fc_y
        mat[0][2] = cc[0]; // cc_x
        mat[1][2] = cc[1]; // cc_y
        mat[2][2] = 1.0;

        if (_input_distorted == false) {
            // using the ros Projection/camera matrix
            mat[0][3] = (ARFloat) _camer_info->P[3];
            mat[1][3] = (ARFloat) _camer_info->P[7];
        }

        for (int i = 0; i < 4; i++)
            this->dist_factor[i] = this->kc[i];
    }
};


void ARToolKitPlusNode::initTrackerMultiMarker(const sensor_msgs::CameraInfoConstPtr& camer_info) {
    trackerMultiMarker_ = boost::shared_ptr<ARToolKitPlus::TrackerMultiMarker>(new ARToolKitPlus::TrackerMultiMarkerImpl<AR_TRACKER_PARAM>(camer_info->width, camer_info->height));
    const char* description = trackerMultiMarker_->getDescription();
    ROS_INFO("%s: compile-time information:\n%s", param_.node_name.c_str(), description);

// set a logger so we can output error messages
    if(logger_ == NULL) logger_ = new MyLogger();
    trackerMultiMarker_->setLogger(logger_);
    trackerMultiMarker_->setPixelFormat(ARToolKitPlus::PIXEL_FORMAT_LUM);

    ARCamera *camera = new ARCamera(camer_info, param_.undist_iterations, param_.distorted_input);
    if (!trackerMultiMarker_->init(camera, param_.pattern_file.c_str(), 1.0f, 1000.0f)) {
        ROS_ERROR("ERROR: init() failed");
    }

}

void ARToolKitPlusNode::initTrackerSingleMarker(const sensor_msgs::CameraInfoConstPtr& camer_info) {
    trackerSingleMarker_ = boost::shared_ptr<ARToolKitPlus::TrackerSingleMarker>(new ARToolKitPlus::TrackerSingleMarkerImpl<AR_TRACKER_PARAM>(camer_info->width, camer_info->height));
    const char* description = trackerSingleMarker_->getDescription();
    ROS_INFO("%s: compile-time information:\n%s", param_.node_name.c_str(), description);

// set a logger so we can output error messages
    trackerSingleMarker_->setLogger(logger_);
    trackerSingleMarker_->setPixelFormat(ARToolKitPlus::PIXEL_FORMAT_LUM);

    ARCamera *camera = new ARCamera(camer_info, param_.undist_iterations, param_.distorted_input);
    if (!trackerSingleMarker_->init(camera, 1.0f, 1000.0f)) {
        ROS_ERROR("ERROR: init() failed");
    }

}

void ARToolKitPlusNode::updateParameterTrackerSingleMarker(const sensor_msgs::CameraInfoConstPtr& camer_info) {

    ARCamera * camera = (ARCamera *) trackerSingleMarker_->getCamera();
    if (camera->isInputDistorted() != param_.distorted_input) {
        delete camera;
        camera = new ARCamera(camer_info, param_.undist_iterations, param_.distorted_input);
        trackerSingleMarker_->setCamera(camera);
    }

// define size of the marker
    trackerSingleMarker_->setPatternWidth(param_.patternWidth);

// the marker in the BCH test image has a thin border...
    if (param_.borderWidth > 0) {
        trackerSingleMarker_->setBorderWidth(param_.borderWidth);
    } else {
        trackerSingleMarker_->setBorderWidth(param_.useBCH ? 0.125f : 0.250f);
    }

// set a threshold. alternatively we could also activate automatic thresholding
    if (param_.edge_threshold > 0) {
        trackerSingleMarker_->activateAutoThreshold(false);
        trackerSingleMarker_->setThreshold(param_.edge_threshold);
    } else {
        trackerSingleMarker_->activateAutoThreshold(true);
    }
// let's use lookup-table undistortion for high-speed
// note: LUT only works with images up to 1024x1024
    trackerSingleMarker_->setUndistortionMode((ARToolKitPlus::UNDIST_MODE) param_.undist_mode);

// RPP is more robust than ARToolKit's standard pose estimator
    trackerSingleMarker_->setPoseEstimator((ARToolKitPlus::POSE_ESTIMATOR) param_.pose_estimation_mode);

// switch to simple ID based markers
// use the tool in tools/IdPatGen to generate markers
    trackerSingleMarker_->setMarkerMode(param_.useBCH ? ARToolKitPlus::MARKER_ID_BCH : ARToolKitPlus::MARKER_ID_SIMPLE);

// do the OpenGL camera setup
//glMatrixMode(GL_PROJECTION)
//glLoadMatrixf(tracker->getProjectionMatrix());
}

void ARToolKitPlusNode::updateParameterTrackerMultiMarker(const sensor_msgs::CameraInfoConstPtr& camer_info) {

    ARCamera * camera = (ARCamera *) trackerMultiMarker_->getCamera();
    if (camera->isInputDistorted() != param_.distorted_input) {
        delete camera;
        camera = new ARCamera(camer_info, param_.undist_iterations, param_.distorted_input);
        trackerMultiMarker_->setCamera(camera);
    }

    trackerMultiMarker_->setUseDetectLite(param_.use_multi_marker_lite_detection);

// the marker in the BCH test image has a thin border...
    if (param_.borderWidth > 0) {
        trackerMultiMarker_->setBorderWidth(param_.borderWidth);
    } else {
        trackerMultiMarker_->setBorderWidth(param_.useBCH ? 0.125f : 0.250f);
    }

// set a threshold. alternatively we could also activate automatic thresholding
    if (param_.edge_threshold > 0) {
        trackerMultiMarker_->activateAutoThreshold(false);
        trackerMultiMarker_->setThreshold(param_.edge_threshold);
    } else {
        trackerMultiMarker_->activateAutoThreshold(true);
    }
// let's use lookup-table undistortion for high-speed
// note: LUT only works with images up to 1024x1024
    trackerMultiMarker_->setUndistortionMode((ARToolKitPlus::UNDIST_MODE) param_.undist_mode);

// RPP is more robust than ARToolKit's standard pose estimator
    trackerMultiMarker_->setPoseEstimator((ARToolKitPlus::POSE_ESTIMATOR) param_.pose_estimation_mode);

// switch to simple ID based markers
// use the tool in tools/IdPatGen to generate markers
    trackerMultiMarker_->setMarkerMode(param_.useBCH ? ARToolKitPlus::MARKER_ID_BCH : ARToolKitPlus::MARKER_ID_SIMPLE);

// do the OpenGL camera setup
//glMatrixMode(GL_PROJECTION)
//glLoadMatrixf(tracker->getProjectionMatrix());

}


void ARToolKitPlusNode::imageCallback(const sensor_msgs::ImageConstPtr& image_msg, const sensor_msgs::CameraInfoConstPtr& camer_info_) {
    callback_counter_++;
    if((callback_counter_ % (param_.skip_frames+1) ) != 0) {
        return;
    }
    cv_bridge::CvImagePtr img;
    try {
        img = cv_bridge::toCvCopy(image_msg, sensor_msgs::image_encodings::MONO8);
    } catch (cv_bridge::Exception& e) {
        ROS_ERROR("cv_bridge exception: %s", e.what());
        return;
    }
    if (param_.tracker_single_marker) {
        if(trackerSingleMarker_ == NULL) initTrackerSingleMarker(camer_info_);

        updateParameterTrackerSingleMarker(camer_info_);
        ARToolKitPlus::ARMarkerInfo* arMarkerInfo;
        int nNumMarkers;
        int markerId = trackerSingleMarker_->calc(img->image.data, param_.nPattern, param_.nUpdateMatrix, &arMarkerInfo, &nNumMarkers);
        float conf = (float) trackerSingleMarker_->getConfidence();
        arMarkerInfo_.resize(nNumMarkers);
        for(int i = 0; i < arMarkerInfo_.size(); i++) {
            arMarkerInfo_[i] = arMarkerInfo[i];
        }
    }
    if (param_.tracker_multi_marker) {
        if(trackerMultiMarker_ == NULL) initTrackerMultiMarker(camer_info_);
        updateParameterTrackerMultiMarker(camer_info_);
        int nNumMarkers = trackerMultiMarker_->calc(img->image.data);

        arMultiMarkerInfo_ = trackerMultiMarker_->getMultiMarkerConfig();
        arMarkerInfo_.clear();
        arMarkerInfo_.reserve(trackerMultiMarker_->getNumDetectedMarkers());
        /// Sort out marker which are part of multi marker patterns
        for(int i = 0; i < trackerMultiMarker_->getNumDetectedMarkers(); i++) {
            bool isSingleMarker = true;
            const ARToolKitPlus::ARMarkerInfo &singleMarker = trackerMultiMarker_->getDetectedMarker(i);
            for(int j = 0; (j < arMultiMarkerInfo_->marker_num) && isSingleMarker; j++) {
                const ARToolKitPlus::ARMultiEachMarkerInfoT &multiMarker = arMultiMarkerInfo_->marker[j];
                if(singleMarker.id == multiMarker.patt_id) {
                    isSingleMarker = false;
                }
            }
            if(isSingleMarker) {
                arMarkerInfo_.push_back(singleMarker);
            }
        }
    }

    estimatePoses(image_msg->header);

    publishTf();

    if (param_.show_camera_image_) {
        cv::Mat img_debug;
        cvtColor(img->image, img_debug, CV_GRAY2BGR);
        generateDebugImage(img_debug);
        cv::imshow( param_.node_name + std::string(" - debug"), img_debug);
        cv::waitKey(5);
    }
}

void ARToolKitPlusNode::estimatePoses(const std_msgs::Header &header) {

    ARFloat center[2];
    center[0] = 0;
    center[1] = 0;
    ARFloat pose[3][4];
    tf::Transform trans;
    tf::StampedTransform st;
    char frame[0xFF];
    markerTransforms_.clear();

    if(trackerMultiMarker_) {

        if( arMultiMarkerInfo_->marker_num > 0)
        {
            const ARFloat *p = trackerMultiMarker_->getModelViewMatrix();
            for(int r = 0; r < 3; r++) {
                pose[r][0] = p[r+0];
                pose[r][1] = p[r+4];
                pose[r][2] = p[r+8];
                pose[r][3] = p[r+12];
            }
            matrix2Tf(pose, trans);
            std::string child_frame = tf::resolve(param_.tf_prefix, param_.pattern_frame);
            st = tf::StampedTransform(trans.inverse(), header.stamp, child_frame, header.frame_id);
            markerTransforms_.push_back(st);
        }
    }
    for(std::vector<ARToolKitPlus::ARMarkerInfo>::iterator it =  arMarkerInfo_.begin(); it != arMarkerInfo_.end(); it++) {
        ARToolKitPlus::ARMarkerInfo &m = *it;
        if (it->id < 0)
            continue;
        sprintf(frame, "t%i", m.id);
        trackerMultiMarker_->executeSingleMarkerPoseEstimator(&m, center, param_.patternWidth, pose);
        matrix2Tf(pose, trans);
        std::string child_frame = tf::resolve(param_.tf_prefix, frame);
        st = tf::StampedTransform(trans, header.stamp, header.frame_id, child_frame);
        markerTransforms_.push_back(st);
    }
}

void ARToolKitPlusNode::init() {
    if (param_.show_camera_image_) {
        cv::namedWindow( param_.node_name + std::string(" - debug"), 1);
    }
}


