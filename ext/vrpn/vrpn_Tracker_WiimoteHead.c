/** @file	vrpn_Tracker_WiimoteHead.C
	@brief	Implementation of Wii Remote head tracking filter driver

	@date	2009-2010

	@author
	Ryan Pavlik
	<rpavlik@iastate.edu> and <abiryan@ryand.net>
	http://academic.cleardefinition.com/
	Iowa State University Virtual Reality Applications Center
	Human-Computer Interaction Graduate Program

	See ASME paper WINVR2010-3771 for more details on this system.
*/
/*
	Copyright Iowa State University 2009-2010
	Distributed under the Boost Software License, Version 1.0.
	(See accompanying comment below or copy at
	http://www.boost.org/LICENSE_1_0.txt)

	Boost Software License - Version 1.0 - August 17th, 2003

	Permission is hereby granted, free of charge, to any person or organization
	obtaining a copy of the software and accompanying documentation covered by
	this license (the "Software") to use, reproduce, display, distribute,
	execute, and transmit the Software, and to prepare derivative works of the
	Software, and to permit third-parties to whom the Software is furnished to
	do so, all subject to the following:

	The copyright notices in the Software and this entire statement, including
	the above license grant, this restriction and the following disclaimer,
	must be included in all copies of the Software, in whole or in part, and
	all derivative works of the Software, unless such copies or derivative
	works are solely in the form of machine-executable object code generated by
	a source language processor.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
	SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
	FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
	ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
	DEALINGS IN THE SOFTWARE.
*/



// Local Includes
#include "quat.h"                       // for q_xyz_quat_type, q_vec_copy, etc
#include "vrpn_Connection.h"            // for vrpn_Connection, etc
#include "vrpn_Tracker_WiimoteHead.h"
#include "vrpn_Types.h"                 // for vrpn_float64

// Standard includes
#include <math.h>                       // for tan, atan2, sqrt
#include <stdio.h>                      // for NULL, fprintf, stderr
#include <algorithm>                    // for swap
#include <iostream>                     // for operator<<, basic_ostream, etc

#undef	VERBOSE

/// We need isnan but don't want to go crazy on requirements
/// hence the following
inline static bool wm_isnan(const double x) {
	return (x != x);
}

/// @name Constants
/// @{
const double two = 2;

// Some stats source: http://wiibrew.org/wiki/Wiimote#IR_Camera
const double xResSensor = 1024.0, yResSensor = 768.0;

/// Field of view experimentally determined at Iowa State University
/// March 2010
const double fovX = Q_DEG_TO_RAD(43.0), fovY = Q_DEG_TO_RAD(32.00);
//const double fovX = Q_DEG_TO_RAD(45.0), fovY = (fovX / xResSensor) * yResSensor;

const double radPerPx = fovX / xResSensor;
const double cvtDistToAngle = radPerPx / two;
/// @}

/** @brief Utility function to set a quat equal to the identity rotation
 */
#define MAKE_IDENTITY_QUAT(dest) \
	dest[0] = dest[1] = dest[2] = 0; dest[3] = 1

/** @brief Utility function to set a 3-vector equal to the zero vector
 */
#define MAKE_NULL_VEC(dest) \
	dest[0] = dest[1] = dest[2] = 0


vrpn_Tracker_WiimoteHead::vrpn_Tracker_WiimoteHead(const char* name,
        vrpn_Connection* trackercon,
        const char* wiimote,
        float update_rate,
        float led_spacing) :
	vrpn_Tracker(name, trackercon),
	d_name(wiimote),
	d_update_interval(update_rate ? (1.0 / update_rate) : 1.0 / 60.0),
	d_blobDistance(led_spacing),
	d_flipState(FLIP_UNKNOWN),
	d_points(0),
	d_ana(NULL),
	d_contact(false),
	d_lock(false),
	d_updated(false),
	d_gravDirty(true) {
	// If the name is NULL, we're done.
	if (wiimote == NULL) {
		d_name = NULL;
		fprintf(stderr, "vrpn_Tracker_WiimoteHead: "
		        "Can't start without a valid specified Wiimote device!");
		return;
	}

	setup_wiimote();

	//--------------------------------------------------------------------
	// Whenever we get a connection, set a flag so we make sure to send an
	// update. Set up a handler to do this.
	register_autodeleted_handler(d_connection->register_message_type(vrpn_got_connection),
	                             handle_connection, this);

	//--------------------------------------------------------------------
	// Set the current matrix to identity, the current timestamp to "now",
	// the current matrix to identity in case we never hear from the Wiimote.
	// Also, set the updated flag to send a single report
	reset();

	// put a little z translation as a saner default
	d_currentPose.xyz[2] = 1;

	// Set up our initial "default" pose to make sure everything is
	// safely initialized before our first report.
	_convert_pose_to_tracker();
}

vrpn_Tracker_WiimoteHead::~vrpn_Tracker_WiimoteHead(void) {

	// If the analog pointer is NULL, we're done.
	if (d_ana == NULL) {
		return;
	}

	// Turn off the callback handler
	int	ret;
	ret = d_ana->unregister_change_handler(this, handle_analog_update);

	// Delete the analog device.
	delete d_ana;
	d_ana = NULL;
}

/** Reset the current pose to identity and store it into the tracker
    position/quaternion location, and set the updated flag.
*/
void vrpn_Tracker_WiimoteHead::reset() {
	_reset_gravity();
	_reset_pose();
	_reset_points();
}

void vrpn_Tracker_WiimoteHead::setup_wiimote() {
	if (d_ana) {
		// Turn off the callback handler and delete old analog
		// if we already have an analog source
		d_ana->unregister_change_handler(this, handle_analog_update);
		delete d_ana;
		d_ana = NULL;
	}

	// Open the analog device and point the remote at it.
	// If the name starts with the '*' character, use the server
	// connection rather than making a new one.
	if (d_name[0] == '*') {
		d_ana = new vrpn_Analog_Remote(&(d_name[1]), d_connection);
	} else {
		d_ana = new vrpn_Analog_Remote(d_name);
	}

	if (d_ana == NULL) {
		fprintf(stderr, "vrpn_Tracker_WiimoteHead: "
		        "Can't open Analog %s\n", d_name);
		return;
	}

	// register callback
	int ret = d_ana->register_change_handler(this, handle_analog_update);
	if (ret == -1) {
		fprintf(stderr, "vrpn_Tracker_WiimoteHead: "
		        "Can't setup change handler on Analog %s\n", d_name);
		delete d_ana;
		d_ana = NULL;
		return;
	}

	// will notify when we catch the first report.
	d_contact = false;
}

void vrpn_Tracker_WiimoteHead::mainloop() {
	struct timeval now;

	// Call generic server mainloop, since we are a server
	server_mainloop();

	// Mainloop() the wiimote to get fresh values
	if (d_ana != NULL) {
		d_ana->mainloop();
	}

	// See if we have new data, or if it has been too long since our last
	// report.  Send a new report in either case.
	vrpn_gettimeofday(&now, NULL);
	double interval = vrpn_TimevalDurationSeconds(now, d_prevtime);

	if (_should_report(interval)) {
		// Figure out the new matrix based on the current values and
		// the length of the interval since the last report
		update_pose();

		report();
	}
}

void vrpn_Tracker_WiimoteHead::update_pose() {
	q_xyz_quat_type newPose;

	// Start at the identity pose
	MAKE_NULL_VEC(newPose.xyz);
	MAKE_IDENTITY_QUAT(newPose.quat);

	// If our gravity vector has changed and it's not 0,
	// we need to update our gravity correction transform.
	if (d_gravDirty && _have_gravity()) {
		_update_gravity_moving_avg();
	}

	// Update pose estimate
	_update_2_LED_pose(newPose);

	if (d_lock) {
		// Gravity correction
		if (_have_gravity()) {
			q_xyz_quat_compose(&d_currentPose, &d_currentPose, &d_gravityXform);
		}

		if (d_flipState == FLIP_UNKNOWN) {
			_update_flip_state();
			if (d_flipState == FLIP_180) {
				return; // must throw away first update after setting flip to 180
			}
		}

		// Copy final pose into the tracker position and quaternion structures.
		_convert_pose_to_tracker();
	}
}

void vrpn_Tracker_WiimoteHead::report() {
	// pack and deliver tracker report;
	if (d_connection) {
		char      msgbuf[1000];
		int       len = encode_to(msgbuf);
		if (d_connection->pack_message(len, vrpn_Tracker::timestamp,
		                               position_m_id, d_sender_id, msgbuf,
		                               vrpn_CONNECTION_LOW_LATENCY)) {
			fprintf(stderr, "vrpn_Tracker_WiimoteHead: "
			        "cannot write message: tossing\n");
		}
	} else {
		fprintf(stderr, "vrpn_Tracker_WiimoteHead: "
		        "No valid connection\n");
	}

	// We just sent a report, so reset the time
	vrpn_gettimeofday(&d_prevtime, NULL);
	d_updated = false;
}

// static
void vrpn_Tracker_WiimoteHead::handle_analog_update(void* userdata, const vrpn_ANALOGCB info) {
	vrpn_Tracker_WiimoteHead* wh = (vrpn_Tracker_WiimoteHead*)userdata;

	if (!wh) {
		return;
	}
	if (!wh->d_contact) {
#ifdef VERBOSE
		fprintf(stderr, "vrpn_Tracker_WiimoteHead: "
		        "got first report from Wiimote!\n");
#endif
	}

	int i, firstchan;
	// Grab all the blobs
	for (i = 0; i < 4; i++) {
		firstchan = i * 3 + 4;
		// -1 should signal a missing blob, but experimentally
		// we sometimes get 0 instead
		if (info.channel[firstchan] > 0
		        && info.channel[firstchan + 1] > 0
		        && info.channel[firstchan + 2] > 0) {
			wh->d_vX[i] = info.channel[firstchan];
			wh->d_vY[i] = info.channel[firstchan + 1];
			wh->d_vSize[i] = info.channel[firstchan + 2];
			wh->d_points = i + 1;
		} else {
			break;
		}
	}

	wh->d_contact = true;
	wh->d_updated = true;

	bool newgrav = false;

	// Grab gravity
	for (i = 0; i < 3; i++) {
		if (info.channel[1 + i] != wh->d_vGrav[i]) {
			newgrav = true;
			break;
		}
	}

	if (newgrav) {
		if (!wh->d_gravDirty) {
			// only slide back the previous gravity if we actually used it once.
			q_vec_copy(wh->d_vGravAntepenultimate, wh->d_vGravPenultimate);
			q_vec_copy(wh->d_vGravPenultimate, wh->d_vGrav);
		}
		for (i = 0; i < 3; i++) {
			wh->d_vGrav[i] = info.channel[1 + i];
		}
		wh->d_gravDirty = true;
	}

	// Store the time of the report into the tracker's timestamp field.
	wh->vrpn_Tracker::timestamp = info.msg_time;
}

// static
int vrpn_Tracker_WiimoteHead::handle_connection(void* userdata, vrpn_HANDLERPARAM) {
	vrpn_Tracker_WiimoteHead* wh = reinterpret_cast<vrpn_Tracker_WiimoteHead*>(userdata);

	// Indicate that we should send a report with whatever we have.
	wh->d_updated = true;

	// Always return 0 here, because nonzero return means that the input data
	// was garbage, not that there was an error. If we return nonzero from a
	// vrpn_Connection handler, it shuts down the connection.
	return 0;
}

void vrpn_Tracker_WiimoteHead::_update_gravity_moving_avg() {
	// Moving average of last three gravity vectors
	/// @todo replace/supplement gravity moving average with Kalman filter
	q_vec_type movingAvg = Q_NULL_VECTOR;

	q_vec_copy(movingAvg, d_vGrav);
	q_vec_add(movingAvg, movingAvg, d_vGravPenultimate);
	q_vec_add(movingAvg, movingAvg, d_vGravAntepenultimate);
	q_vec_scale(movingAvg, 0.33333, movingAvg);

	// reset gravity transform
	MAKE_IDENTITY_QUAT(d_gravityXform.quat);
	MAKE_NULL_VEC(d_gravityXform.xyz);

	q_vec_type regulargravity = Q_NULL_VECTOR;
	regulargravity[2] = 1;

	q_from_two_vecs(d_gravityXform.quat, movingAvg, regulargravity);
	d_gravDirty = false;
}

void vrpn_Tracker_WiimoteHead::_update_2_LED_pose(q_xyz_quat_type & newPose) {
	if (d_points != 2) {
		// we simply stop updating our pos+orientation if we lost LED's
		// TODO: right now if we don't have exactly 2 points we lose the lock
		d_lock = false;
		d_flipState = FLIP_UNKNOWN;
		return;
	}

	// TODO right now only handling the 2-LED glasses

	d_lock = true;
	double rx, ry, rz;
	rx = ry = rz = 0;

	double X0, X1, Y0, Y1;

	X0 = d_vX[0];
	X1 = d_vX[1];
	Y0 = d_vY[0];
	Y1 = d_vY[1];

	if (d_flipState == FLIP_180) {
		/// If the first report of a new tracking lock indicated that
		/// our "up" vector had no positive y component, we have the
		/// points in the wrong order - flip them around.
		/// This uses the assumption that the first time we see the glasses,
		/// they ought to be right-side up (a reasonable assumption for
		/// head tracking)
		std::swap(X0, X1);
		std::swap(Y0, Y1);
	}

	const double dx = X0 - X1;
	const double dy = Y0 - Y1;
	const double dist = sqrt(dx * dx + dy * dy);
	const double angle = dist * cvtDistToAngle;
	// Note that this is an approximation, since we don't know the
	// distance/horizontal position.  (I think...)

	const double headDist = (d_blobDistance / 2.0) / tan(angle);

	// Translate the distance along z axis, and tilt the head
	newPose.xyz[2] = headDist;      // translate along Z
	rz = atan2(dy, dx);     // rotate around Z

	// Find the sensor pixel of the line of sight - directly between
	// the LEDs
	const double avgX = (X0 + X1) / 2.0;
	const double avgY = (Y0 + Y1) / 2.0;

	/// @todo For some unnerving reason, in release builds, avgX tends to become NaN/undefined
	/// However, any kind of inspection (such as the following, or even a simple cout)
	/// appears to prevent the issue.  This makes me uneasy, but I won't argue with
	/// what is working.
	if (wm_isnan(avgX)) {
		std::cerr << "NaN detected in avgX: X0 = " << X0 << ", X1 = " << X1 << std::endl;
		return;
	}

	if (wm_isnan(avgY)) {
		std::cerr << "NaN detected in avgY: Y0 = " << Y0 << ", Y1 = " << Y1 << std::endl;
		return;
	}

	// b is the virtual depth in the sensor from a point to the full sensor
	// used for finding similar triangles to calculate x/y translation
	const double bHoriz = xResSensor / 2 / tan(fovX / 2);
	const double bVert = yResSensor / 2 / tan(fovY / 2);

	// World head displacement (X and Y) from a centered origin at
	// the calculated distance from the sensor
	newPose.xyz[0] = headDist * (avgX - xResSensor / 2) / bHoriz;
	newPose.xyz[1] = headDist * (avgY - yResSensor / 2) / bVert;

	// set the quat. part of our pose with rotation angles
	q_from_euler(newPose.quat, rz, ry, rx);

	// Apply the new pose
	q_vec_copy(d_currentPose.xyz, newPose.xyz);
	q_copy(d_currentPose.quat, newPose.quat);
}

void vrpn_Tracker_WiimoteHead::_update_flip_state() {
	if (d_flipState != FLIP_UNKNOWN) {
		return;
	}

	q_vec_type upVec = {0, 1, 0};

	q_xform(upVec, d_currentPose.quat, upVec);
	if (upVec[1] < 0) {
		// We are upside down - we will need to rotate 180 about the sensor Z
		// Must recalculate now.
#ifdef VERBOSE
		fprintf(stderr, "vrpn_Tracker_WiimoteHead: d_flipState = FLIP_180\n");
#endif
		d_flipState = FLIP_180;
		update_pose();
	} else {
		// OK, we are fine - there is a positive Y component to our up vector
#ifdef VERBOSE
		fprintf(stderr, "vrpn_Tracker_WiimoteHead: d_flipState = FLIP_NORMAL\n");
#endif
		d_flipState = FLIP_NORMAL;
	}
}

void vrpn_Tracker_WiimoteHead::_convert_pose_to_tracker() {
	q_vec_copy(pos, d_currentPose.xyz); // set position;
	q_copy(d_quat, d_currentPose.quat); // set orientation
}


void vrpn_Tracker_WiimoteHead::_reset_gravity() {
	MAKE_NULL_VEC(d_gravityXform.xyz);
	MAKE_IDENTITY_QUAT(d_gravityXform.quat);

	MAKE_NULL_VEC(d_vGrav);
	MAKE_NULL_VEC(d_vGravPenultimate);
	MAKE_NULL_VEC(d_vGravAntepenultimate);

	// Default earth gravity is (0, 1, 0)
	d_vGravAntepenultimate[2] = d_vGravPenultimate[2] = d_vGrav[2] = 1;

	d_gravDirty = true;
}

void vrpn_Tracker_WiimoteHead::_reset_points() {
	d_vX[0] = d_vX[1] = d_vX[2] = d_vX[3] = -1;
	d_vY[0] = d_vY[1] = d_vY[2] = d_vY[3] = -1;
	d_vSize[0] = d_vSize[1] = d_vSize[2] = d_vSize[3] = -1;
	d_points = 0;


	d_flipState = FLIP_UNKNOWN;
	d_lock = false;
}

void vrpn_Tracker_WiimoteHead::_reset_pose() {
	// Reset to the identity pose
	MAKE_NULL_VEC(d_currentPose.xyz);
	MAKE_IDENTITY_QUAT(d_currentPose.quat);

	vrpn_gettimeofday(&d_prevtime, NULL);

	// Set the updated flag to send a report
	d_updated = true;
	d_flipState = FLIP_UNKNOWN;
	d_lock = false;

	// Convert the matrix into quaternion notation and copy into the
	// tracker pos and quat elements.
	_convert_pose_to_tracker();
}

bool vrpn_Tracker_WiimoteHead::_should_report(double elapsedInterval) const {
	// If we've gotten new wiimote reports since our last report, return true.
	if (d_updated) {
		return true;
	}

	// If it's been more than our max interval, send an update anyway
	if (elapsedInterval >= d_update_interval) {
		return true;
	}

	// Not time has elapsed, and nothing has changed, so return false.
	return false;
}

bool vrpn_Tracker_WiimoteHead::_have_gravity() const {
	return (d_vGrav[0] != 0 || d_vGrav[1] != 1 || d_vGrav[2] != 0);
}