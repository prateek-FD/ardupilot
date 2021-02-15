/*
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */
/*
  support for autotune of multirotors. Based on original autotune code from ArduCopter, written by Leonard Hall
  Converted to a library by Andrew Tridgell
 */
#pragma once

#include <AP_HAL/AP_HAL.h>
#include <AC_AttitudeControl/AC_AttitudeControl.h>
#include <AC_AttitudeControl/AC_PosControl.h>
#include <AP_Math/AP_Math.h>

#define AUTOTUNE_AXIS_BITMASK_ROLL            1
#define AUTOTUNE_AXIS_BITMASK_PITCH           2
#define AUTOTUNE_AXIS_BITMASK_YAW             4

#define AUTOTUNE_SUCCESS_COUNT                4     // The number of successful iterations we need to freeze at current gains

// Auto Tune message ids for ground station
#define AUTOTUNE_MESSAGE_STARTED 0
#define AUTOTUNE_MESSAGE_STOPPED 1
#define AUTOTUNE_MESSAGE_SUCCESS 2
#define AUTOTUNE_MESSAGE_FAILED 3
#define AUTOTUNE_MESSAGE_SAVED_GAINS 4
#define AUTOTUNE_MESSAGE_TESTING 5

#define AUTOTUNE_ANNOUNCE_INTERVAL_MS 2000

#define AUTOTUNE_DWELL_CYCLES                6

class AC_AutoTune
{
public:
    // constructor
    AC_AutoTune();

    // main run loop
    virtual void run();

    // save gained, called on disarm
    virtual void save_tuning_gains();

    // stop tune, reverting gains
    void stop();

    // reset Autotune so that gains are not saved again and autotune can be run again.
    void reset() {
        mode = UNINITIALISED;
        axes_completed = 0;
    }

    // var_info for holding Parameter information
    static const struct AP_Param::GroupInfo var_info[];

protected:

    // axis that can be tuned
    enum AxisType {
        ROLL = 0,
        PITCH = 1,
        YAW = 2
    };

    //
    // methods that must be supplied by the vehicle specific subclass
    //
    virtual bool init(void) = 0;

    // get pilot input for desired climb rate
    virtual float get_pilot_desired_climb_rate_cms(void) const = 0;

    // get pilot input for designed roll and pitch, and yaw rate
    virtual void get_pilot_desired_rp_yrate_cd(float &roll_cd, float &pitch_cd, float &yaw_rate_cds) = 0;

    // init pos controller Z velocity and accel limits
    virtual void init_z_limits() = 0;

    // log PIDs at full rate for during twitch
    virtual void log_pids() = 0;

    // start tune - virtual so that vehicle code can add additional pre-conditions
    virtual bool start(void);

    // return true if we have a good position estimate
    virtual bool position_ok();

    enum at_event {
        EVENT_AUTOTUNE_INITIALISED   =  0,
        EVENT_AUTOTUNE_OFF           =  1,
        EVENT_AUTOTUNE_RESTART       =  2,
        EVENT_AUTOTUNE_SUCCESS       =  3,
        EVENT_AUTOTUNE_FAILED        =  4,
        EVENT_AUTOTUNE_REACHED_LIMIT =  5,
        EVENT_AUTOTUNE_PILOT_TESTING =  6,
        EVENT_AUTOTUNE_SAVEDGAINS    =  7
    };

    // write a log event
    virtual void Log_Write_Event(enum at_event id) = 0;

    // internal init function, should be called from init()
    bool init_internals(bool use_poshold,
                        AC_AttitudeControl *attitude_control,
                        AC_PosControl *pos_control,
                        AP_AHRS_View *ahrs_view,
                        AP_InertialNav *inertial_nav);

    // main state machine to level vehicle, perform a test and update gains
    // directly updates attitude controller with targets
    void control_attitude();

    //
    // methods to load and save gains
    //

    // backup original gains and prepare for start of tuning
    void backup_gains_and_initialise();

    // switch to use original gains
    void load_orig_gains();

    // switch to gains found by last successful autotune
    void load_tuned_gains();

    // load gains used between tests. called during testing mode's update-gains step to set gains ahead of return-to-level step
    void load_intra_test_gains();

    // load gains for next test.  relies on axis variable being set
    virtual void load_test_gains();

    // get intra test rate I gain for the specified axis
    virtual float get_intra_test_ri(AxisType test_axis) = 0;

    // get tuned rate I gain for the specified axis
    virtual float get_tuned_ri(AxisType test_axis) = 0;

    // get tuned yaw rate d gain
    virtual float get_tuned_yaw_rd() = 0;

    // test init and run methods that should be overridden for each vehicle
    virtual void test_init() = 0;
    virtual void test_run(AxisType test_axis, const float dir_sign) = 0;

    // return true if user has enabled autotune for roll, pitch or yaw axis
    bool roll_enabled() const;
    bool pitch_enabled() const;
    bool yaw_enabled() const;

    void twitching_test_rate(float rate, float rate_target, float &meas_rate_min, float &meas_rate_max);
    void twitching_abort_rate(float angle, float rate, float angle_max, float meas_rate_min);
    void twitching_test_angle(float angle, float rate, float angle_target, float &meas_angle_min, float &meas_angle_max, float &meas_rate_min, float &meas_rate_max);
    void twitching_measure_acceleration(float &rate_of_change, float rate_measurement, float &rate_measurement_max);

    // twitch test functions for multicopter
    void twitch_test_init();
    void twitch_test_run(AxisType test_axis, const float dir_sign);

    // update gains for the rate p up tune type
    virtual void updating_rate_p_up_all(AxisType test_axis)=0;

    // update gains for the rate p down tune type
    virtual void updating_rate_p_down_all(AxisType test_axis)=0;

    // update gains for the rate d up tune type
    virtual void updating_rate_d_up_all(AxisType test_axis)=0;

    // update gains for the rate d down tune type
    virtual void updating_rate_d_down_all(AxisType test_axis)=0;

    // update gains for the angle p up tune type
    virtual void updating_angle_p_up_all(AxisType test_axis)=0;

    // update gains for the angle p down tune type
    virtual void updating_angle_p_down_all(AxisType test_axis)=0;

    // returns true if rate P gain of zero is acceptable for this vehicle
    virtual bool allow_zero_rate_p() = 0;

    // get minimum rate P (for any axis)
    virtual float get_rp_min() const = 0;

    // get minimum angle P (for any axis)
    virtual float get_sp_min() const = 0;

    // get minimum rate Yaw filter value
    virtual float get_yaw_rate_filt_min() const = 0;

    // get attitude for slow position hold in autotune mode
    void get_poshold_attitude(float &roll_cd, float &pitch_cd, float &yaw_cd);

    virtual void Log_AutoTune() = 0;
    virtual void Log_AutoTuneDetails() = 0;

    // send message with high level status (e.g. Started, Stopped)
    void update_gcs(uint8_t message_id);

    // send lower level step status (e.g. Pilot overrides Active)
    void send_step_string();

    // convert latest level issue to string for reporting
    const char *level_issue_string() const;

    // convert tune type to string for reporting
    const char *type_string() const;

    // send intermittant updates to user on status of tune
    virtual void do_gcs_announcements() = 0;

    enum struct LevelIssue {
        NONE,
        ANGLE_ROLL,
        ANGLE_PITCH,
        ANGLE_YAW,
        RATE_ROLL,
        RATE_PITCH,
        RATE_YAW,
    };

    // check if current is greater than maximum and update level_problem structure
    bool check_level(const enum LevelIssue issue, const float current, const float maximum);

    // returns true if vehicle is close to level
    bool currently_level();

    // autotune modes (high level states)
    enum TuneMode {
        UNINITIALISED = 0,        // autotune has never been run
        TUNING = 1,               // autotune is testing gains
        SUCCESS = 2,              // tuning has completed, user is flight testing the new gains
        FAILED = 3,               // tuning has failed, user is flying on original gains
    };

    // steps performed while in the tuning mode
    enum StepType {
        WAITING_FOR_LEVEL = 0,    // autotune is waiting for vehicle to return to level before beginning the next twitch
        TESTING           = 1,    // autotune has begun a test and is watching the resulting vehicle movement
        UPDATE_GAINS      = 2     // autotune has completed a test and is updating the gains based on the results
    };

    // mini steps performed while in Tuning mode, Testing step
    enum TuneType {
        RD_UP = 0,                // rate D is being tuned up
        RD_DOWN = 1,              // rate D is being tuned down
        RP_UP = 2,                // rate P is being tuned up
        RP_DOWN = 3,              // rate P is being tuned down
        RFF_UP = 4,               // rate FF is being tuned up
        RFF_DOWN = 5,             // rate FF is being tuned down
        SP_UP = 6,                // angle P is being tuned up
        SP_DOWN = 7,              // angle P is being tuned down
        MAX_GAINS = 8,            // max allowable stable gains are determined
        TUNE_COMPLETE = 9         // Reached end of tuning
    };
    TuneType tune_seq[6];         // holds sequence of tune_types to be performed
    uint8_t tune_seq_curr;        // current tune sequence step

    // type of gains to load
    enum GainType {
        GAIN_ORIGINAL   = 0,
        GAIN_TEST       = 1,
        GAIN_INTRA_TEST = 2,
        GAIN_TUNED      = 3,
    };
    void load_gains(enum GainType gain_type);

    TuneMode mode                : 2;    // see TuneMode for what modes are allowed
    bool     pilot_override      : 1;    // true = pilot is overriding controls so we suspend tuning temporarily
    AxisType axis                : 2;    // current axis being tuned. see AxisType enum
    bool     positive_direction  : 1;    // false = tuning in negative direction (i.e. left for roll), true = positive direction (i.e. right for roll)
    StepType step                : 2;    // see StepType for what steps are performed
    TuneType tune_type           : 4;    // see TuneType
    bool     ignore_next         : 1;    // true = ignore the next test
    bool     twitch_first_iter   : 1;    // true on first iteration of a twitch (used to signal we must step the attitude or rate target)
    bool     use_poshold         : 1;    // true = enable position hold
    bool     have_position       : 1;    // true = start_position is value
    Vector3f start_position;             // target when holding position as an offset from EKF origin in cm in NEU frame
    uint8_t  axes_completed;             // bitmask of completed axes

    // variables
    uint32_t override_time;                         // the last time the pilot overrode the controls
    float    test_rate_min;                         // the minimum angular rate achieved during TESTING_RATE step
    float    test_rate_max;                         // the maximum angular rate achieved during TESTING_RATE step
    float    test_angle_min;                        // the minimum angle achieved during TESTING_ANGLE step
    float    test_angle_max;                        // the maximum angle achieved during TESTING_ANGLE step
    uint32_t step_start_time_ms;                    // start time of current tuning step (used for timeout checks)
    uint32_t level_start_time_ms;                   // start time of waiting for level
    uint32_t step_time_limit_ms;                    // time limit of current autotune process
    int8_t   counter;                               // counter for tuning gains
    float    target_rate, start_rate;               // target and start rate
    float    target_angle, start_angle;             // target and start angles
    float    desired_yaw_cd;                        // yaw heading during tune
    float    rate_max, test_accel_max;              // maximum acceleration variables
    float    step_scaler;                           // scaler to reduce maximum target step
    float    abort_angle;                           // Angle that test is aborted

    LowPassFilterFloat  rotation_rate_filt;         // filtered rotation rate in radians/second

    // backup of currently being tuned parameter values
    float    orig_roll_rp, orig_roll_ri, orig_roll_rd, orig_roll_rff, orig_roll_fltt, orig_roll_sp, orig_roll_accel;
    float    orig_pitch_rp, orig_pitch_ri, orig_pitch_rd, orig_pitch_rff, orig_pitch_fltt, orig_pitch_sp, orig_pitch_accel;
    float    orig_yaw_rp, orig_yaw_ri, orig_yaw_rd, orig_yaw_rff, orig_yaw_rLPF, orig_yaw_fltt, orig_yaw_sp, orig_yaw_accel;
    bool     orig_bf_feedforward;

    // currently being tuned parameter values
    float    tune_roll_rp, tune_roll_rd, tune_roll_sp, tune_roll_accel;
    float    tune_pitch_rp, tune_pitch_rd, tune_pitch_sp, tune_pitch_accel;
    float    tune_yaw_rp, tune_yaw_rLPF, tune_yaw_sp, tune_yaw_accel;
    float    tune_roll_rff, tune_pitch_rff, tune_yaw_rd, tune_yaw_rff;

    uint32_t announce_time;
    float lean_angle;
    float rotation_rate;
    float roll_cd, pitch_cd;

    uint32_t last_pilot_override_warning;

    struct {
        LevelIssue issue{LevelIssue::NONE};
        float maximum;
        float current;
    } level_problem;

    // parameters
    AP_Int8  axis_bitmask;
    AP_Float aggressiveness;
    AP_Float min_d;

    // copies of object pointers to make code a bit clearer
    AC_AttitudeControl *attitude_control;
    AC_PosControl *pos_control;
    AP_AHRS_View *ahrs_view;
    AP_InertialNav *inertial_nav;
    AP_Motors *motors;

    // Functions added for heli autotune

    // Add additional updating gain functions specific to heli
    // generic method used by subclasses to update gains for the rate ff up tune type
    virtual void updating_rate_ff_up_all(AxisType test_axis)=0;
    // generic method used by subclasses to update gains for the rate ff down tune type
    virtual void updating_rate_ff_down_all(AxisType test_axis)=0;
    // generic method used by subclasses to update gains for the max gain tune type
    virtual void updating_max_gains_all(AxisType test_axis)=0;

    // Feedforward test used to determine Rate FF gain
    void rate_ff_test_init();
    void rate_ff_test_run(float max_angle_cds, float target_rate_cds);

    // dwell test used to perform frequency dwells for rate gains
    void dwell_test_init(float filt_freq);
    void dwell_test_run(uint8_t freq_resp_input, float dwell_freq, float &dwell_gain, float &dwell_phase);

    // dwell test used to perform frequency dwells for angle gains
    void angle_dwell_test_init(float filt_freq);
    void angle_dwell_test_run(float dwell_freq, float &dwell_gain, float &dwell_phase);

    // determines the gain and phase for a dwell
    void determine_gain(float tgt_rate, float meas_rate, float freq, float &gain, float &phase, bool &cycles_complete, bool funct_reset);

    // determines the gain and phase for a dwell
    void determine_gain_angle(float command, float tgt_angle, float meas_angle, float freq, float &gain, float &phase, float &max_accel, bool &cycles_complete, bool funct_reset);

    uint8_t  ff_test_phase;                         // phase of feedforward test
    float    test_command_filt;                     // filtered commanded output
    float    test_rate_filt;                        // filtered rate output
    float    command_out;
    float    test_tgt_rate_filt;                    // filtered target rate
    float    filt_target_rate;
    bool     ff_up_first_iter   : 1;       //true on first iteration of ff up testing
    float    test_gain[20];                             // gain of output to input
    float    test_freq[20];
    float    test_phase[20];
    float    dwell_start_time_ms;
    uint8_t  freq_cnt;
    uint8_t  freq_cnt_max;
    float    curr_test_freq;
    bool     dwell_complete;
    Vector3f start_angles;

    LowPassFilterFloat  command_filt;               // filtered command
    LowPassFilterFloat  target_rate_filt;            // filtered target rotation rate in radians/second

    struct max_gain_data {
        float freq;
        float phase;
        float gain;
        float max_allowed;
    };

    max_gain_data max_rate_p;
    max_gain_data max_rate_d;
};
