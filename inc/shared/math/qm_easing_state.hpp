/**
*
*
*   QMath: C++ Only, a little state object for cleaner code that utilizes one of the supplied easing methods.
*
*
**/
#pragma once


#ifdef __cplusplus

/**
*   @brief  A little state object for cleaner code that utilizes one of the supplied easing methods.
**/
struct QMEaseState {
public:
    //! Type of eases states.
    enum QMEaseStateType {
        //! The ease is inwardly active.
        QM_EASE_STATE_TYPE_IN,
        //! The ease is outwardly active.
        QM_EASE_STATE_TYPE_OUT,
        //! The ease is in/out active.
        QM_EASE_STATE_TYPE_IN_OUT,
    };
    //! State of the ease.
    enum QMEaseStateMode {
        //! The ease is planned ahead of time.
        QM_EASE_STATE_MODE_WAITING,
        //! The ease has already passed the current time.
        QM_EASE_STATE_MODE_DONE,
        //! The ease is inwardly active.
        QM_EASE_STATE_MODE_EASING_IN,
        //! The ease is outwardly active.
        QM_EASE_STATE_MODE_EASING_OUT,
        //! The ease is in/out active.
        QM_EASE_STATE_MODE_EASING_IN_OUT,
    } mode;

    // Callback.
    using QMEaseStateMethod = const double( * )( const double &lerpFraction );

private:
    //! The absolute time this ease started.
    QMTime timeStart;
    //! The absolute time this ease should be done by.
    QMTime timeEnd;
    //! The duration.
    QMTime timeDuration;
    //! Determined by the easing methods, actual normalized factor value of the ease.
    double easeFactor;

public:
    /**
    *   @return Number of the 'easing type mode' we're actively operating on.
    **/
    inline const QMEaseStateMode &GetEaseType() {
        return mode;
    }
    /**
    *   @return Absolute Time that this ease is meant to start off at.
    **/
    inline const QMTime &GetStartTime() const { return timeStart; }
    /**
    *   @return Absolute Time that this ease is meant to finish at.
    **/
    inline const QMTime &GetEndTime() const { return timeEnd; }
    /**
    *   @return Relative time of duration.
    **/
    inline const QMTime &GetDurationTime() const { return timeDuration; }

    /**
    *   @return Normalized easing factor. (Determined by easing methods, 0 by default.
    **/
    inline const double &GetEasingFactort() const { return easeFactor; }

private:
    /**
    *   @brief  Easing time range.
    **/
    enum QMEaseTimeRange {
        QM_EASE_STATE_TIME_RANGE_POST,
        QM_EASE_STATE_TIME_RANGE_WITHIN,
        QM_EASE_STATE_TIME_RANGE_PAST,
    };

    /**
    *   @return QM_EASE_STATE_TIME_RANGE_POST if still building up, time < startTime,
    *           QM_EASE_STATE_TIME_RANGE_WITHIN if we're time >= startTime and time < endTime,
    *           and QM_EASE_STATE_TIME_RANGE_POST if we're > endTime
    **/
    static const QMEaseTimeRange DetermineTimeRange( const QMTime &time, const QMEaseState &easeState ) {
        // If eased in, we're done, return 1.0
        if ( time > easeState.timeEnd ) {
            return QM_EASE_STATE_TIME_RANGE_PAST;
        }
        // If eased out, we're still waiting.
        if ( time < easeState.timeStart ) {
            return QM_EASE_STATE_TIME_RANGE_POST;
        }
        // Easing.
        return QM_EASE_STATE_TIME_RANGE_WITHIN;
    }

    /**
    *   @brief  Performs the easing.
    **/
    const double &EaseForTime( const QMTime &time, const QMEaseStateType &easeType, QMEaseStateMethod easingCallback ) {
        // Still awaiting.
        if ( DetermineTimeRange( time, *this ) < QM_EASE_STATE_TIME_RANGE_WITHIN ) {
            mode = QM_EASE_STATE_MODE_WAITING;
            return 0.f;
        }
        // Past ease end time.
        if ( DetermineTimeRange( time, *this ) > QM_EASE_STATE_TIME_RANGE_WITHIN ) {
            mode = QM_EASE_STATE_MODE_DONE;
            return 1.f;
        }
        // We're easing in.
        switch ( easeType ) {
        case QM_EASE_STATE_TYPE_IN:
            mode = QM_EASE_STATE_MODE_EASING_IN;
            break;
        case QM_EASE_STATE_TYPE_OUT:
            mode = QM_EASE_STATE_MODE_EASING_OUT;
            break;
        default:
        case QM_EASE_STATE_TYPE_IN_OUT:
            mode = QM_EASE_STATE_MODE_EASING_IN_OUT;
            break;
        }
        // Lerp fraction, clamp for discrepancies.
        const double lerpFraction = QM_Clampd( (double)( time.Milliseconds() - timeStart.Milliseconds() ) / (double)timeDuration.Milliseconds(), 0., 1. );
        // Call upon the callback.
        easeFactor = QM_Clampd( easingCallback( lerpFraction ), 0., 1. );
        // Return result.
        return easeFactor;
    }

public:
    /**
    *   @brief  Initializes a new easing state.
    **/
    static const QMEaseState new_ease_state( const QMTime &timeStart, const QMTime &duration ) {
        QMEaseState qmState;
        qmState.mode = QM_EASE_STATE_MODE_WAITING;
        qmState.timeStart = timeStart;
        qmState.timeEnd = timeStart + duration;
        qmState.timeDuration = duration;
        return qmState;
    }

    /**
    *   @return The resulting factor for this moment in time using the specified easing callback method.
    **/
    const double &EaseIn( const QMTime &time, QMEaseStateMethod easingCallback ) {
        return EaseForTime( time, QM_EASE_STATE_TYPE_IN, easingCallback );
    }
    /**
    *   @return The resulting factor for this moment in time using the specified easing callback method.
    **/
    const double &EaseOut( const QMTime &time, QMEaseStateMethod easingCallback ) {
        return EaseForTime( time, QM_EASE_STATE_TYPE_OUT, easingCallback );
    }
    /**
    *   @return The resulting factor for this moment in time using the specified easing callback method.
    **/
    const double &EaseInout( const QMTime &time, QMEaseStateMethod easingCallback ) {
        return EaseForTime( time, QM_EASE_STATE_TYPE_IN_OUT, easingCallback );
    }
};

#endif