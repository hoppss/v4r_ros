/***************************************************************************
 *   Copyright (C) 2014 by Markus Bader                                    *
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

#include "opencv2/imgproc/imgproc.hpp"
#include <opencv/cv.h>
#include <numeric>      // std::inner_product
#include <v4r_ellipses/v4r_ellipses.h>
#include <v4r_ellipses/v4r_ellipses_defaults.h>
#include <boost/foreach.hpp>
#include "v4r_utils/canny.h"

using namespace V4R;


EllipsesDetection::~EllipsesDetection()
{
    if(param_ != NULL) delete param_;
}

EllipsesDetection::EllipsesDetection(Parameters *param)
    :param_(param), loop_count(0) {
}

void EllipsesDetection::createEllipseCanditates () {
    ellipses_.clear();
    ellipses_.resize(contours_.size());
    cv::Mat cameraMatrix, distCoeffs;
    for(unsigned int i = 0; i < contours_.size(); i++) {
        unsigned int count = contours_[i].size();
        Ellipse &ellipse = ellipses_[i];
        ellipse.init();
        ellipse.id = i;
        if(param_->filter_convex) {
            cv::approxPolyDP( cv::Mat (contours_[i]), *ellipse.polygon, param_->threshold_polygon, true );
            ellipse.boxContour = cv::boundingRect( cv::Mat(*ellipse.polygon) );
            //cv::minEnclosingCircle( (cv::Mat)*ellipse.polygon, ellipse.centerContour, ellipse.radiusContour );
        }
        ellipse.contourDistort->resize(count);
        ellipse.contourUndistort->resize(count);
        for(unsigned int j = 0; j < count; j++) {
            (*ellipse.contourDistort)[j] = contours_[i][j];
            (*ellipse.contourUndistort)[j] = contours_[i][j];
        }
        if(param_->distorted_input) {
            cv::undistortPoints(*ellipse.contourDistort, *ellipse.contourUndistort, camera_.cameraMatrix, camera_.distCoeffs, cv::Mat(), camera_.projectionMatrix);
        }
    }
}


EllipsesDetection::DetectionState EllipsesDetection::filterContour (Ellipse &ellipse) {
    if(ellipse.contourDistort->size() < param_->threshold_contour_min_points) {
        ellipse.detection = INVALID_CONTOUR_POINTS;
        return ellipse.detection;
    }
    if(param_->filter_convex && !cv::isContourConvex(*ellipse.polygon)) {
        ellipse.detection = INVALID_CONTOUR_CONVEX;
        return ellipse.detection;
    }
    return ellipse.detection;
}

void EllipsesDetection::edge_detection(const cv::Mat &m) {
    imgGray_ = m;
    switch(param_->edge_detection) {
    case EDGE_DETECTION_THRESHOLD:
        imgEdges_ = m >= param_->threshold_edge_detection1;
        break;
    case EDGE_DETECTION_CANNY:
        cv::blur(imgGray_, imgBlured_, cv::Size(3,3) );
        V4R::Canny(imgBlured_, imgEdges_, imgGradient_, imgDirection_, imgSobelDx_, imgSobelDy_, param_->threshold_edge_detection1, param_->threshold_edge_detection2, param_->kernel_size_edge_detection);
        break;
    }
}

void EllipsesDetection::contour_detection() {
    switch(param_->edge_linking) {
    case EDGE_LINKING_OPENCV_APPROX_NONE:
        cv::findContours ( imgEdges_, contours_, CV_RETR_LIST, CV_CHAIN_APPROX_NONE );
        break;
    case EDGE_LINKING_OPENCV_APPROX_SIMPLE:
        cv::findContours( imgEdges_, contours_, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE );
        break;
    case EDGE_LINKING_V4R_SIMPLE:
        if(param_->edge_detection != EDGE_DETECTION_CANNY) break;
        contour_detector_.Init(imgGray_.cols, imgGray_.rows);
        contour_detector_.Perform(imgEdges_.data, V4R::Contour::MODE_SIMPLE, imgGradient_.data);
        contour_detector_.getContours(contours_);
        break;
    case EDGE_LINKING_V4R_COMPLEX:
        if(param_->edge_detection != EDGE_DETECTION_CANNY) break;
        contour_detector_.Init(imgGray_.cols, imgGray_.rows);
        contour_detector_.Perform(imgEdges_.data, V4R::Contour::MODE_COMPLEX, imgGradient_.data);
        contour_detector_.getContours(contours_);
        break;
    case EDGE_LINKING_V4R_CONTOUR:
        if(param_->edge_detection != EDGE_DETECTION_CANNY) break;
        contour_detector_.Init(imgGray_.cols, imgGray_.rows);
        contour_detector_.Perform(imgEdges_.data, V4R::Contour::MODE_CONTOUR, imgGradient_.data);
        contour_detector_.getContours(contours_);
        break;
    }
}

void EllipsesDetection::fit_ellipses_opencv (const cv::Mat &m, const cv::Mat cameraMatrix, const cv::Mat distCoeffs, const cv::Mat projectionMatrix ) {
    loop_count++;
    if(param_->distorted_input) {
        cameraMatrix.convertTo(camera_.cameraMatrix, CV_64F) ;
        distCoeffs.convertTo(camera_.distCoeffs, CV_64F) ;
        projectionMatrix(cv::Rect(0,0,3,3)).convertTo(camera_.projectionMatrix, CV_64F) ;
        camera_.projectionMatrix(1,1) = camera_.projectionMatrix(0,0);
    } else {
        projectionMatrix(cv::Rect(0,0,3,3)).convertTo(camera_.cameraMatrix, CV_64F) ;
        camera_.distCoeffs = (cv::Mat_<double>(1,5) << 0, 0, 0, 0, 0);
        projectionMatrix(cv::Rect(0,0,3,3)).convertTo(camera_.projectionMatrix, CV_64F) ;
    }

    edge_detection(m);
    contour_detection();
    createEllipseCanditates();

    for(unsigned int i = 0; i < ellipses_.size(); i++) {
        Ellipse &ellipse = ellipses_[i];
        if(filterContour (ellipse) != VALID) continue;
        cv::Mat pointsf(*ellipse.contourUndistort);
        ellipse.boxEllipse = fitEllipse ( pointsf );
        ellipse.radiusEllipseMax = MAX( ellipse.boxEllipse.size.width , ellipse.boxEllipse.size.height) / 2.0;
        ellipse.radiusEllipseMin = MIN( ellipse.boxEllipse.size.width , ellipse.boxEllipse.size.height) / 2.0;
        if(filterEllipse (ellipse) != VALID) continue;
        if(filterContourMean(ellipse)  != VALID) continue;
    }
}

EllipsesDetection::DetectionState EllipsesDetection::filterEllipse(Ellipse &ellipse) {
    if(ellipse.detection != VALID) return ellipse.detection;
    double boxEllipseRatio = ellipse.radiusEllipseMin / ellipse.radiusEllipseMax;
    if ( boxEllipseRatio < param_->threshold_ring_ratio ) {
        ellipse.detection = INVALID_ROTATED_RECT_RATIO;
        return ellipse.detection;
    }
    return ellipse.detection;
}

void EllipsesDetection::createRings() {
    if(!param_->filter_rings) return;
    for(std::vector<Ellipse>::iterator a = ellipses_.begin(); a != ellipses_.end(); a++) {
        if((a->innerRing >= 0)  || (a->outerRing >= 0) || (a->detection != VALID)) continue;
        double threshold_center = a->radiusEllipseMax * param_->threshold_ring_center;
        for(std::vector<Ellipse>::iterator b = ellipses_.begin(); b != ellipses_.end(); b++) {
            if((a->id == b->id) || (b->innerRing != -1)  || (b->outerRing != -1) || (b->detection != VALID) ) {
                continue;
            }
            double d = cv::norm(a->boxEllipse.center - b->boxEllipse.center);
            if(d < threshold_center) {
                Ellipse *outer = &(*a), *inner = &(*b);
                if(a->radiusEllipseMax < b->radiusEllipseMax)  outer = &(*b), inner =  &(*a);
                double ratioRadiusRingMax = fabs(inner->radiusEllipseMax / outer->radiusEllipseMax-0.5);
                double ratioRadiusRingMin = fabs(inner->radiusEllipseMax / outer->radiusEllipseMax-0.5);
                if((ratioRadiusRingMax > param_->threshold_ring_ratio) || (ratioRadiusRingMin > param_->threshold_ring_ratio)) {
                    continue;
                }
                outer->innerRing = inner->id;
                inner->outerRing = outer->id;
                inner->detection = INVALID_IS_INNER_RING;
                break;
            }
        }
        if((a->innerRing == -1)  && (a->outerRing == -1)) a->detection = INVALID_NO_RING;
    }
}

void EllipsesDetection::next() {
    contours_.clear();
    ellipses_.clear();
    perception_.clear();
}

EllipsesDetection::DetectionState  EllipsesDetection::filterContourMean(Ellipse &ellipse) {
    if(!param_->filter_contour_mean || (ellipse.detection != VALID)) return ellipse.detection;
    const std::vector<cv::Point2f> &contour = *(ellipse.contourUndistort.get());
    ellipse.distances = boost::shared_ptr<std::vector<double> >(new std::vector<double> );
    std::vector<double > &distances = *(ellipse.distances.get());
    const cv::Point2f &pc = ellipse.boxEllipse.center;
    double angle = M_PI/180.0 * (double) ellipse.boxEllipse.angle;
    double ca = cos(angle), sa = sin(angle);
    double a = ellipse.boxEllipse.size.width/2., b = ellipse.boxEllipse.size.height/2.;
    double sum = 0;
    distances.reserve(contour.size());
    for(unsigned int i = 0; i < contour.size(); i++) {
        double dx = contour[i].x - pc.x, dy = contour[i].y - pc.y;
        double u = ca*dx + sa*dy, v = -sa*dx + ca*dy;
        cv::Point2d p(contour[i].x - pc.x, contour[i].y - pc.y);
        double d = (u*u)/(a*a) + (v*v)/(b*b);
        distances.push_back(d);
        sum += d;
        // http://stackoverflow.com/questions/11041547/finding-the-distance-of-a-point-to-an-ellipse-wether-its-inside-or-outside-of-e
    }
    double mean = sum / distances.size();
    double diff = fabs((mean - 1.));
    if(diff > param_->threshold_contour_mean) {
        ellipse.detection = INVALID_CONTOUR_MEAN;
    }
    return ellipse.detection;
}


void EllipsesDetection::estimatePoses() {
    if(param_->pose_estimation == POSE_ESTIMATION_OFF) return;
    Pose pose;
    cv::Point2f pi[4];
    if(param_->circle_diameter <= 0) return;
    double radius = param_->circle_diameter / 2.0;
    cv::Mat objectPoints  = (cv::Mat_<double>(5,3) << 0, 0, 0, -radius, +radius, 0, +radius, +radius, 0, +radius, -radius, 0, -radius, -radius, 0);
    for(std::vector<Ellipse>::iterator ellipse = ellipses_.begin(); ellipse != ellipses_.end(); ellipse++) {
        if(ellipse->detection != VALID) continue;
        ellipse->boxEllipse.points(pi);
        cv::Point2f& pc = ellipse->boxEllipse.center;
        cv::Mat imagePoints = (cv::Mat_<double>(5,2) << pc.x, pc.y, pi[0].x, pi[0].y, pi[1].x, pi[1].y, pi[2].x, pi[2].y, pi[3].x, pi[3].y);
        switch(param_->pose_estimation) {
        case POSE_ESTIMATION_SOLVEPNP:
            cv::solvePnP(objectPoints, imagePoints, camera_.cameraMatrix, camera_.distCoeffs, pose.rvec, pose.tvec);
            pose.roation2Normal();
            break;
        case POSE_ESTIMATION_FROM_ELLIPSE:
            ellipse->cone.set(ellipse->boxEllipse, camera_.projectionMatrix, param_->circle_diameter/2.);
            ellipse->cone.pose(camera_.projectionMatrix, camera_.distCoeffs, pose.nvec, pose.tvec);
            pose.normal2Roation();
            break;
        default:
            continue;
        }
        perception_.push_back(pose);
    }
    markers_.clear();
    for(std::list<Pose>::iterator it = perception_.begin(); it != perception_.end(); it++) {
        Marker m(*it);
        markers_.push_back(m);
    }

}




void EllipsesDetection::ObliqueCone::set(cv::RotatedRect box, cv::Mat_<double> intrinsic, double radius) {
    double a = box.size.width/2., b = box.size.height/2.;
    double angle = M_PI/180.0 * (float) box.angle;
    double ca = cos(angle), sa = sin(angle);
    fx = intrinsic(0,0), fy = intrinsic(1,1);
    cx = intrinsic(0,2), cy = intrinsic(1,2);
    cv::Mat_<double>  Re = (cv::Mat_<double>(2,2) << ca, -sa, sa, ca);
    cv::Mat_<double> ABInvTAB = (cv::Mat_<double>(2,2) << 1./(a*a), 0., 0., 1./(b*b));
    cv::Mat_<double>  X0 = (cv::Mat_<double>(2,1) << box.center.x-cx, box.center.y-cy);
    cv::Mat_<double>  M = Re * ABInvTAB * Re.t();
    cv::Mat_<double>  Mf = X0.t() * M * X0;
    double A = M(0,0);
    double B = M(0,1);
    double C = M(1,1);
    double D = - A * X0(0) - B * X0(1);
    double E = - B * X0(0) - C * X0(1);
    double F = Mf(0,0) - 1.0;
    this->C =  (cv::Mat_<double>(3,3) << A, B, D,
                B, C, E,
                D, E, F);

    double focalLength = (fx+fy)/2.0;
    if(focalLength == 0) return;
    this->radius = radius;
    this->Q = this->C.clone();
    this->Q(0,0) = this->C(0,0);
    this->Q(0,1) = this->C(0,1);
    this->Q(0,2) = this->C(0,2) / (focalLength);
    this->Q(1,0) = this->C(1,0);
    this->Q(1,1) = this->C(1,1);
    this->Q(1,2) = this->C(1,2) / (focalLength);
    this->Q(2,0) = this->C(2,0) / (focalLength);
    this->Q(2,1) = this->C(2,1) / (focalLength);
    this->Q(2,2) = this->C(2,2) / (focalLength*focalLength);

}

void EllipsesDetection::Pose::roation2Normal() {
    cv::Rodrigues(rvec, R);
    nvec.create(3,1);
    nvec = R * (cv::Mat_<double>(3,1) << 0, 0, 1);
    double d0 = cv::norm(tvec - nvec);
    double d1 = cv::norm(tvec + nvec);
    if(d1 < d0) {
        nvec = -nvec;
    }
}
void EllipsesDetection::Pose::normal2Roation() {
    cv::Mat_<double> unitZ = (cv::Mat_<double>(3,1) << 0, 0, 1);
    nvec = nvec/cv::norm(nvec);
    cv::Mat_<double> c2 = nvec;
    cv::Mat_<double> c1 = unitZ.cross(c2);
    cv::Mat_<double> c0 = c1.cross(c2);
    c1 = c1/cv::norm(c1);
    c0 = c0/cv::norm(c0);
    R = (cv::Mat_<double>(3,3) << c0(0), c1(0), c2(0), c0(1), c1(1), c2(1), c0(2), c1(2), c2(2));
    cv::Rodrigues(R, rvec);
}

double EllipsesDetection::ObliqueCone::distance(const cv::Point2d p) {
    cv::Mat_<double> P =  (cv::Mat_<double>(3,1) << p.x, p.y, 1);
    cv::Mat_<double> D = P.t() * C * P;
    double d = D(0,0);
    return d;
}

void EllipsesDetection::ObliqueCone::pose(cv::Mat_<double> intrinsic, cv::Mat_<double> distCoeffs, cv::Mat &nvec, cv::Mat &tvec) {
    cv::eigen(Q, E, V);
    V = V.t();
    double e1 = E.at<double>(0);
    double e2 = E.at<double>(1);
    double e3 = E.at<double>(2);
    double S1[] = {+1,+1,+1,+1,-1,-1,-1,-1};
    double S2[] = {+1,+1,-1,-1,+1,+1,-1,-1};
    double S3[] = {+1,-1,+1,-1,+1,-1,+1,-1};

    double g = sqrt((e2-e3)/(e1-e3));
    double h = sqrt((e1-e2)/(e1-e3));
    N = cv::Mat_<double>::zeros(3,8);
    T = cv::Mat_<double>::zeros(3,8);
    Pi = cv::Mat_<double>::zeros(2,8);
    translations.clear();
    normals.clear();;
    for(int i = 0; i < 8; i++) {

        double z0 =  S3[i] *    (e2 * radius) / sqrt(-e1*e3);
        double Tx =  S2[i] * e3/e2 * h;
        double Ty =  0.;
        double Tz = -S1[i] * e1/e2 * g;
        double Nx =  S2[i] * h;
        double Ny =  0.;
        double Nz = -S1[i] * g;

        cv::Mat_<double> t(T, cv::Rect(i, 0, 1, 3));
        cv::Mat_<double> n(N, cv::Rect(i, 0, 1, 3));
        t = (z0 * V * (cv::Mat_<double>(3,1) << Tx, Ty, Tz));
        n = (V *  (cv::Mat_<double>(3,1) << Nx, Ny, Nz));;
        cv::Mat_<double> Pc = intrinsic * t;

        // Projection
        Pi(0,i) = Pc(0)/Pc(2);
        Pi(1,i) = Pc(1)/Pc(2);

        if((t(2) > 0) && (n(2)<0)) {
            translations.push_back(cv::Point3d(t(0), t(1), t(2)));
            normals.push_back(cv::Vec3d(n(0), n(1), n(2)));
        }
    }
    tvec = (cv::Mat_<double>(3,1) << translations[0].x, translations[0].y, translations[0].z);
    nvec = (cv::Mat_<double>(3,1) << normals[0][0], normals[0][1], normals[0][2]);
    /*
    if(d0 > 0) {
    }  else {
    tvec = (cv::Mat_<double>(3,1) << trans[0].x, trans[0].y, trans[0].z);
    nvec = (cv::Mat_<double>(3,1) << -nomals[0][0], -nomals[0][1], -nomals[0][2]);
    }*/
}
