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


#include <v4r_ellipses/v4r_ellipses.h>
#include <v4r_ellipses/v4r_ellipses_defaults.h>

using namespace V4R;

EllipsesDetection::Parameters::Parameters()
: debug(V4R_ELLIPSES_DEFAULT_DEBUG)
, distorted_input(V4R_ELLIPSES_DEFAULT_DISTORTED_INPUT)
, edge_detection( (EdgeDetection) V4R_ELLIPSES_DEFAULT_EDGE_DETECTION)
, threshold_edge_detection1(V4R_ELLIPSES_DEFAULT_THRESHOLD_EDGE_DETECTION1)
, threshold_edge_detection2(V4R_ELLIPSES_DEFAULT_THRESHOLD_EDGE_DETECTION2)
, kernel_size_edge_detection(V4R_ELLIPSES_DEFAULT_KERNEL_SIZE_EDGE_DETECTION)
, edge_linking((EdgeLinking) V4R_ELLIPSES_DEFAULT_EDGE_LINKING)
, threshold_contour_min_points(V4R_ELLIPSES_DEFAULT_THRESHOLD_CONTROUR_MIN_POINTS)
, threshold_polygon(V4R_ELLIPSES_DEFAULT_THRESHOLD_POLYGON)
, filter_convex(V4R_ELLIPSES_DEFAULT_FILTER_CONVEX)
, threshold_rotated_rect_ratio(V4R_ELLIPSES_DEFAULT_THRESHOLD_ROTATED_RECT_RATIO)
, threshold_min_radius(V4R_ELLIPSES_DEFAULT_THRESHOLD_MIN_RADIUS)
, threshold_max_radius(V4R_ELLIPSES_DEFAULT_THRESHOLD_MAX_RADIUS)
, filter_rings(V4R_ELLIPSES_DEFAULT_FILTER_RING)
, threshold_ring_center(V4R_ELLIPSES_DEFAULT_THRESHOLD_RING_CENTER)
, threshold_ring_ratio(V4R_ELLIPSES_DEFAULT_THRESHOLD_RING_RATIO)
, filter_contour_mean(V4R_ELLIPSES_DEFAULT_FILTER_CONTOUR_MEAN)
, threshold_contour_mean(V4R_ELLIPSES_DEFAULT_THRESHOLD_CONTOUR_MEAN)
, pose_estimation( (PoseEstimation) V4R_ELLIPSES_DEFAULT_ESTIMATE_POSE)
, circle_diameter(V4R_ELLIPSES_DEFAULT_CIRCLE_DIAMETER){
}
