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
#include <v4r_artoolkitplus/TransformArrayStamped.h>

int main(int argc, char **argv) {
    ros::init(argc, argv, "arMarker");
    ros::NodeHandle n;
    ARToolKitPlusNode ar(n);
    ros::spin();
    return 0;
}

int ARToolKitPlusNode::matrix2Tf(const ARFloat M[3][4], tf::Transform &transform) {
    tf::Matrix3x3 R(M[0][0], M[0][1], M[0][2], M[1][0], M[1][1], M[1][2], M[2][0], M[2][1], M[2][2]);
    tf::Vector3 T(M[0][3], M[1][3], M[2][3]);
    //transform = tf::Transform(R, T); // this causes a TF to MSG: Quaternion Not Properly Normalized message
    tf::Quaternion quat;
    R.getRotation(quat);
    transform = tf::Transform(quat, T);
}


void ARToolKitPlusNode::publishTf() {
    for(std::list<tf::StampedTransform>::iterator it =  markerTransforms_.begin(); it != markerTransforms_.end(); it++) {
        transformBroadcaster_.sendTransform(*it);
    }
}


void ARToolKitPlusNode::publishPerceptions (const std_msgs::Header &header) {
    if(pub_perceptions_.getNumSubscribers() < 1) return;
    v4r_artoolkitplus::TransformArrayStamped msg;
    if(markerTransforms_.size() > 0) {
        msg.header = header;
        msg.child_frame_id.resize(markerTransforms_.size());
        msg.transform.resize(markerTransforms_.size());
        std::list<tf::StampedTransform>::iterator it =  markerTransforms_.begin();
        for(unsigned int i; i < markerTransforms_.size(); it++, i++) {
            geometry_msgs::Vector3 &desT = msg.transform[i].translation;
            geometry_msgs::Quaternion &desQ = msg.transform[i].rotation;
            tf::Vector3 &srcT = it->getOrigin();
            tf::Quaternion srcQ = it->getRotation();
            desT.x = srcT.x(), desT.y = srcT.y(), desT.z = srcT.z();
            desQ.x = srcQ.x(), desQ.y = srcQ.y(), desQ.z = srcQ.z(), desQ.w = srcQ.w();
            msg.child_frame_id[i] = it->child_frame_id_;
        }
        pub_perceptions_.publish(msg);
    }
}

