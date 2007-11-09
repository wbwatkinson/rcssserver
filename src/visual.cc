// -*-c++-*-

/***************************************************************************
                                   visual.cc
                     Classes for building visual messages
                             -------------------
    begin                : 5-AUG-2002
    copyright            : (C) 2002 by The RoboCup Soccer Server
                           Maintenance Group.
    email                : sserver-admin@lists.sourceforge.net
***************************************************************************/

/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU LGPL as published by the Free Software  *
 *   Foundation; either version 2 of the License, or (at your option) any  *
 *   later version.                                                        *
 *                                                                         *
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "visual.h"
#include "serializer.h"
#include "serializerplayerstdv1.h"
#include "object.h"
#include "player.h"
#include "coach.h"
#include "field.h"

namespace rcss
{

/*!
//===================================================================
//
//  CLASS: VisualSendor
//
//  DESC: Base class for the visual protocol.
//
//===================================================================
*/

VisualSender::VisualSender( std::ostream & transport )
    :  Sender( transport )
{}

VisualSender::~VisualSender()
{}

/*!
//===================================================================
//
//  CLASS: VisualSenderPlayer
//
//  DESC: Base class for the visual protocol for players.
//
//===================================================================
*/

// const double VisualSenderPlayer::UNUM_FAR_LENGTH = 20.0;
// const double VisualSenderPlayer::UNUM_TOOFAR_LENGTH = 40.0;
// const double VisualSenderPlayer::TEAM_FAR_LENGTH = 40.0;
// const double VisualSenderPlayer::TEAM_TOOFAR_LENGTH = 60.0;

VisualSenderPlayer::Factory&
VisualSenderPlayer::factory()
{
    static Factory rval;
    return rval;
}

VisualSenderPlayer::VisualSenderPlayer( const Params& params )
    : VisualSender( params.m_transport ),
      m_ser( params.m_ser ),
      m_self( params.m_self ),
      m_stadium( params.m_stadium ),
      m_sendcnt( 0 )
{}

VisualSenderPlayer::~VisualSenderPlayer()
{}

/*!
//===================================================================
//
//  CLASS: VisualSensorPlayerV1
//
//  DESC: Class for the version 1* visual protocol.  This version is
//        completely unused as far as I am aware of, but it is here
//        none the less, just in case there is someone somewhere
//        still using it.
//
//        * It's version 1 to the simualtor in it's current form.
//        From what I know the original simulator was written in
//        lisp and the first C++ version was actually version 3.  I
//        don't know if the protocol was compatible with previous
//        versions, so this may well be the version 3 protocol.
//
//===================================================================
*/

VisualSenderPlayerV1::VisualSenderPlayerV1( const Params & params )
    : VisualSenderPlayer( params )
{}

VisualSenderPlayerV1::~VisualSenderPlayerV1()
{}

void
VisualSenderPlayerV1::sendVisual()
{
    incSendCount();

    if ( sendCount() >= self().visSend() )
        resetSendCount();
    else
        return;

    serializer().serializeVisualBegin( transport(), stadium().time() );
    if( self().highquality() )
    {
        M_send_flag = &VisualSenderPlayerV1::sendHighFlag;
        M_send_ball = &VisualSenderPlayerV1::sendHighBall;
        M_send_player = &VisualSenderPlayerV1::sendHighPlayer;
        M_serialize_line = &VisualSenderPlayerV1::serializeHighLine;
    }
    else
    {
        M_send_flag = &VisualSenderPlayerV1::sendLowFlag;
        M_send_ball = &VisualSenderPlayerV1::sendLowBall;
        M_send_player = &VisualSenderPlayerV1::sendLowPlayer;
        M_serialize_line = &VisualSenderPlayerV1::serializeLowLine;
    }
    sendFlags();
    sendBalls();
    sendPlayers();
    sendLines();
    serializer().serializeVisualEnd( transport() );
    transport() << std::ends << std::flush;
}

void
VisualSenderPlayerV1::sendFlags()
{
    const std::vector< PObject * >::const_iterator end = stadium().landmarks().end();
    for ( std::vector< PObject * >::const_iterator it = stadium().landmarks().begin();
          it != end;
          ++it )
    {
        if ( (*it)->objectVersion() <= self().version() )
        {
            sendFlag( **it );
        }
    }
}

void
VisualSenderPlayerV1::sendBalls()
{
    if ( stadium().ball().objectVersion() <= self().version() )
    {
        sendBall( stadium().ball() );
    }
}

void
VisualSenderPlayerV1::sendPlayers()
{
    const Stadium::PlayerCont::const_iterator end = stadium().players().end();
    for ( Stadium::PlayerCont::const_iterator p = stadium().players().begin();
          p != end;
          ++p )
    {
        if ( *p != &self()
             && (*p)->alive() != DISABLE
             && (*p)->objectVersion() <= self().version() )
        {
            sendPlayer( *(*p) );
        }
    }
}


void
VisualSenderPlayerV1::sendLines()
{
    int line_count = 0;
    int max_line_count;
    int min_line_count;
    if ( std::fabs( self().pos().x ) < ServerParam::PITCH_LENGTH*0.5
         && std::fabs( self().pos().y ) < ServerParam::PITCH_WIDTH*0.5 )
    {
        min_line_count = max_line_count = 1;
    }
    else
    {
        max_line_count = 2;
        min_line_count = 0;
    }

    if ( line_count < max_line_count
         && stadium().field.line_l.objectVersion() <= self().version() )
    {
        if ( sendLine( stadium().field.line_l ) )
            ++line_count;
    }
    if ( line_count < max_line_count
         && stadium().field.line_r.objectVersion() <= self().version() )
    {
        if( sendLine( stadium().field.line_r ) )
            ++line_count;
    }
    if ( line_count < max_line_count
         && stadium().field.line_t.objectVersion() <= self().version() )
    {
        if ( sendLine( stadium().field.line_t ) )
            ++line_count;
    }
    if ( line_count < max_line_count
         && stadium().field.line_b.objectVersion() <= self().version() )
    {
        if ( sendLine( stadium().field.line_b ) )
            ++line_count;
    }
    if ( line_count < min_line_count )
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << ": Error rendering visual: not enough lines\n"
                  << "Player = " << self() << std::endl;
    }
}

void
VisualSenderPlayerV1::sendLowFlag( const PObject & flag )
{
    const double ang = calcRadDir( flag );

    if ( std::fabs( ang ) < self().visibleAngle() * 0.5 )
    {
        serializer().serializeVisualObject( transport(),
                                            calcName( flag ),
                                            calcDegDir( ang ) );
    }
    else if ( calcUnQuantDist( flag ) <= self().vis_distance )
    {
        serializer().serializeVisualObject( transport(),
                                            calcCloseName( flag ),
                                            calcDegDir( ang ) );
    }
}

void
VisualSenderPlayerV1::sendHighFlag( const PObject & flag )
{
    const double ang = calcRadDir( flag );
    //const double un_quant_dist = calcUnQuantDist( flag );
    double un_quant_dist = self().pos().distance2( flag.pos() );

    if ( std::fabs( ang ) < self().visibleAngle() * 0.5 )
    {
        un_quant_dist = std::sqrt( un_quant_dist );
        const double quant_dist
            = calcQuantDist( un_quant_dist, self().landDistQStep() );

        //const double prob = ( ( quant_dist - UNUM_FAR_LENGTH )
        //                      / ( UNUM_TOOFAR_LENGTH - UNUM_FAR_LENGTH ) );
        const double prob = ( ( quant_dist - self().unumFarLength() )
                              / ( self().unumTooFarLength() - self().unumFarLength() ) );

        if ( decide( prob ) )
        {
            serializer().serializeVisualObject( transport(),
                                                calcName( flag ),
                                                quant_dist,
                                                calcDegDir( ang ) );
        }
        else
        {
            double dist_chg, dir_chg;
            calcVel( PVector(), flag.pos(),
                     un_quant_dist, quant_dist,
                     dist_chg, dir_chg );
            serializer().serializeVisualObject( transport(),
                                                calcName( flag ),
                                                quant_dist,
                                                calcDegDir( ang ),
                                                dist_chg,
                                                dir_chg );
        }
    }
    else if ( un_quant_dist <= self().vis_distance2 )
    {
        //un_quant_dist = std::sqrt( un_quant_dist );
        serializer().serializeVisualObject( transport(),
                                            calcCloseName( flag ),
                                            calcQuantDist( std::sqrt( un_quant_dist ),
                                                           self().landDistQStep() ),
                                            calcDegDir( ang ) );
    }
}

void
VisualSenderPlayerV1::sendLowBall( const MPObject & ball )
{
    const double ang = calcRadDir( ball );

    if( std::fabs( ang ) < self().visibleAngle() * 0.5 )
    {
        serializer().serializeVisualObject( transport(),
                                            calcName( ball ),
                                            calcDegDir( ang ) );
    }
    else if( calcUnQuantDist( ball ) <= self().vis_distance )
    {
        serializer().serializeVisualObject( transport(),
                                            calcCloseName( ball ),
                                            calcDegDir( ang ) );
    }
}


void
VisualSenderPlayerV1::sendHighBall( const MPObject & ball )
{
    const double ang = calcRadDir( ball );
    //const double un_quant_dist = calcUnQuantDist( ball );
    double un_quant_dist = self().pos().distance2( ball.pos() );

    if ( std::fabs( ang ) < self().visibleAngle() * 0.5 )
    {
        un_quant_dist = std::sqrt( un_quant_dist );
        const double quant_dist = calcQuantDist( un_quant_dist,
                                                 self().distQStep() );

        //double prob = ( ( quant_dist - UNUM_FAR_LENGTH )
        //                / ( UNUM_TOOFAR_LENGTH - UNUM_FAR_LENGTH ) );
        double prob = ( ( quant_dist - self().unumFarLength() )
                        / ( self().unumTooFarLength() - self().unumFarLength() ) );

        if ( decide( prob ) )
        {
            serializer().serializeVisualObject( transport(),
                                                calcName( ball ),
                                                quant_dist,
                                                calcDegDir( ang ) );
        }
        else
        {
            double dist_chg, dir_chg;
            calcVel( ball.vel(), ball.pos(),
                     un_quant_dist, quant_dist,
                     dist_chg, dir_chg );
            serializer().serializeVisualObject( transport(),
                                                calcName( ball ),
                                                quant_dist,
                                                calcDegDir( ang ),
                                                dist_chg,
                                                dir_chg );
        }
    }
    else if ( un_quant_dist <= self().vis_distance2 )
    {
        serializer().serializeVisualObject( transport(),
                                            calcCloseName( ball ),
                                            calcQuantDist( std::sqrt( un_quant_dist ),
                                                           self().distQStep() ),
                                            calcDegDir( ang ) );
    }
}

void
VisualSenderPlayerV1::sendLowPlayer( const Player & player )
{
    const double ang = calcRadDir( player );
    //const double un_quant_dist = calcUnQuantDist( player );
    const double un_quant_dist2 = self().pos().distance2( player.pos() );

    if ( std::fabs( ang ) < self().visibleAngle() * 0.5 )
    {
        const double quant_dist = calcQuantDist( std::sqrt( un_quant_dist2 ),
                                                 self().distQStep() );

        //double prob = ( ( quant_dist - TEAM_FAR_LENGTH )
        //              / ( TEAM_TOOFAR_LENGTH - TEAM_FAR_LENGTH ) );
        double prob = ( ( quant_dist - self().teamFarLength() )
                        / ( self().teamTooFarLength() - self().teamFarLength() ) );
        if ( decide( prob ) )
        {
            serializer().serializeVisualObject( transport(),
                                                calcTFarName( player ),
                                                calcDegDir( ang ) );
        }
        else
        {
            //prob = ( ( quant_dist - UNUM_FAR_LENGTH )
            //         / ( UNUM_TOOFAR_LENGTH - UNUM_FAR_LENGTH ) );
            prob = ( ( quant_dist - self().unumFarLength() )
                     / ( self().unumTooFarLength() - self().unumFarLength() ) );
            if ( decide( prob ) )
            {
                serializer().serializeVisualObject( transport(),
                                                    calcUFarName( player ),
                                                    calcDegDir( ang ) );
            }
            else
            {
                serializer().serializeVisualObject( transport(),
                                                    calcName( player ),
                                                    calcDegDir( ang ) );
            }
        }
    }
    else if ( un_quant_dist2 <= self().vis_distance2 )
    {
        serializer().serializeVisualObject( transport(),
                                            calcCloseName( player ),
                                            calcDegDir( ang ) );
    }
}


void
VisualSenderPlayerV1::sendHighPlayer( const Player & player )
{
    const double ang = calcRadDir( player );
    //const double un_quant_dist = calcUnQuantDist( player );
    double un_quant_dist = self().pos().distance2( player.pos() );

    if ( std::fabs( ang ) < self().visibleAngle() * 0.5 )
    {
        un_quant_dist = std::sqrt( un_quant_dist );
        const double quant_dist = calcQuantDist( un_quant_dist,
                                                 self().distQStep() );
        //double prob = ( ( quant_dist - TEAM_FAR_LENGTH )
        //              / ( TEAM_TOOFAR_LENGTH - TEAM_FAR_LENGTH ) );
        double prob = ( ( quant_dist - self().teamFarLength() )
                        / ( self().teamTooFarLength() - self().teamFarLength() ) );

        if ( decide( prob ) )
        {
            serializer().serializeVisualObject( transport(),
                                                calcTFarName( player ),
                                                quant_dist,
                                                calcDegDir( ang ) );
        }
        else
        {
            //prob = ( ( quant_dist - UNUM_FAR_LENGTH )
            //         / ( UNUM_TOOFAR_LENGTH - UNUM_FAR_LENGTH ) );
            prob = ( ( quant_dist - self().unumFarLength() )
                     / ( self().unumTooFarLength() - self().unumFarLength() ) );

            if ( decide( prob ) )
            {
                serializePlayer( player,
                                 calcUFarName( player ),
                                 quant_dist,
                                 calcDegDir( ang ) );
            }
            else
            {
                double dist_chg, dir_chg;
                calcVel( player.vel(), player.pos(),
                         un_quant_dist, quant_dist,
                         dist_chg, dir_chg );
                serializePlayer( player,
                                 calcName( player ),
                                 quant_dist,
                                 calcDegDir( ang ),
                                 dist_chg, dir_chg );
            }
        }
    }
    else if ( un_quant_dist <= player.vis_distance2 )
    {
        //un_quant_dist = std::sqrt( un_quant_dist );
        serializer().serializeVisualObject( transport(),
                                            calcCloseName( player ),
                                            calcQuantDist( std::sqrt( un_quant_dist ),
                                                           self().distQStep() ),
                                            calcDegDir( ang ) );
    }
}


bool
VisualSenderPlayerV1::sendLine( const PObject & line )
{
    /*! the angle of an outward pointing normal ( 90degs to ) to
      the line */
    double line_normal;

    //! perp distance of the player from the line
    double player_2_line;

    //! the x of y value of where the line starts
    double line_start;

    //! the x or y value of where the line stops
    double line_stop;

    //! a flag to specify if the line is vertical or horizontal
    bool vert;

    /*! be very carefull here.  The lines pos.x is actually it's
      distance from the center of the field, not neccesarily it's
      x position. */

    //! left line
    if ( line.pos().x == - ServerParam::PITCH_LENGTH*0.5 )
    {
        line_normal = M_PI;
        if ( self().pos().x < line.pos().x )
            line_normal = 0.0;
        player_2_line = line.pos().x - self().pos().x;
        line_start = - ServerParam::PITCH_WIDTH*0.5;
        line_stop = ServerParam::PITCH_WIDTH*0.5;
        vert = true;
    }
    //! right line
    else if ( line.pos().x == ServerParam::PITCH_LENGTH*0.5 )
    {
        line_normal = 0.0;
        if( self().pos().x > line.pos().x )
            line_normal = M_PI;
        player_2_line = line.pos().x - self().pos().x;
        line_start = - ServerParam::PITCH_WIDTH*0.5;
        line_stop = ServerParam::PITCH_WIDTH*0.5;
        vert = true;
    }
    //! top line
    else if ( line.pos().x == - ServerParam::PITCH_WIDTH*0.5 )
    {
        line_normal = -M_PI*0.5;
        if ( self().pos().y < line.pos().x )
            line_normal = M_PI*0.5;
        player_2_line = line.pos().x - self().pos().y;
        line_start = - ServerParam::PITCH_LENGTH*0.5;
        line_stop = ServerParam::PITCH_LENGTH*0.5;
        vert = false;
    }
    //! bottom line
    else if ( line.pos().x == ServerParam::PITCH_WIDTH*0.5 )
    {
        line_normal = M_PI*0.5;
        if ( self().pos().y > line.pos().x )
            line_normal = -M_PI*0.5;
        player_2_line = line.pos().x - self().pos().y;
        line_start = - ServerParam::PITCH_LENGTH*0.5;
        line_stop = ServerParam::PITCH_LENGTH*0.5;
        vert = false;
    }
    else
    {
        std::cerr << __FILE__ << ": " << __LINE__
                  << ": Error, unknown line: " << line << std::endl;
        return false;
    }

    //! angle between the players line of sight and the line's normal
    double sight_2_line_ang = calcLineRadDir( line_normal );

    /*! if the angle between the line of sight and the line's norm
      is not within -90.0 and 90 degrees then the player is
      looking parallel or away from the line, thus it cannot be
      visible. */
    if ( std::fabs( sight_2_line_ang ) >= M_PI*0.5 )
    {
        return false;
    }

    /*! this gives us the x or y offset from the player for where
      their line of sight intersects the line */
    double line_intersect = player_2_line * std::tan( sight_2_line_ang );

    /*! this calculates the actual x or y value for where the line
      of sight intersects the line.  Because the y axis is
      inverted, we need to use -line_intersect if the line is
      vertical. */
    if ( vert )
        line_intersect = self().pos().y - line_intersect;
    else
        line_intersect += self().pos().x;

    /*! If the point that the players line of sight intersects the
      line beyond it's beginning or end then the player wont see
      this line */
    if ( line_intersect < line_start
         || line_intersect > line_stop )
    {
        return false;
    }

    serializeLine( calcName( line ),
                   calcLineDir( sight_2_line_ang ),
                   sight_2_line_ang, player_2_line );
    return true;
}

void
VisualSenderPlayerV1::serializeLowLine( const std::string & name,
                                        const int dir,
                                        const double &, const double & )
{
    serializer().serializeVisualObject( transport(), name, dir );
}

void
VisualSenderPlayerV1::serializeHighLine( const std::string & name,
                                         const int dir,
                                         const double & sight_2_line_ang,
                                         const double & player_2_line )
{
    double dist = calcLineDist( sight_2_line_ang, player_2_line,
                                self().landDistQStep() );
    serializer().serializeVisualObject( transport(), name, dist, dir );
}

void
VisualSenderPlayerV1::calcVel( const PVector & obj_vel,
                               const PVector & obj_pos,
                               const double & un_quant_dist,
                               const double & quant_dist,
                               double& dist_chg,
                               double& dir_chg ) const
{
    if ( un_quant_dist != 0.0 )
    {
        const PVector vtmp = obj_vel - self().vel();
        PVector etmp = obj_pos - self().pos();
        etmp /= un_quant_dist;

        dist_chg = vtmp.x * etmp.x + vtmp.y * etmp.y;
//         dir_chg = RAD2DEG * ( vtmp.y * etmp.x
//                               - vtmp.x * etmp.y ) / un_quant_dist;
        dir_chg = vtmp.y * etmp.x - vtmp.x * etmp.y;
        dir_chg /= un_quant_dist;
        dir_chg *= RAD2DEG;

        dir_chg = ( dir_chg == 0.0
                    ? 0.0
                    : Quantize( dir_chg, self().dirQStep() ) );
        dist_chg = ( quant_dist
                     * Quantize( dist_chg
                                 / un_quant_dist, 0.02 ) );
    }
    else
    {
        dir_chg = 0.0;
        dist_chg = 0.0;
    }
}

void
VisualSenderPlayerV1::serializePlayer( const Player&,
                                       const std::string & name,
                                       const double & dist,
                                       const int dir,
                                       const double & dist_chg,
                                       const double & dir_chg )
{
    serializer().serializeVisualObject( transport(),
                                        name,
                                        dist, dir,
                                        dist_chg, dir_chg );
}

void
VisualSenderPlayerV1::serializePlayer( const Player&,
                                       const std::string & name,
                                       const double & dist,
                                       const int dir )
{
    serializer().serializeVisualObject( transport(),
                                        name,
                                        dist,
                                        dir );
}

// const
// std::string &
// VisualSenderPlayerV1::calcName( const PObject & obj ) const
// {
//     return obj.name();
// }

// const
// std::string &
// VisualSenderPlayerV1::calcCloseName( const PObject & obj ) const
// {
//     return obj.closeName();
// }

// const
// std::string &
// VisualSenderPlayerV1::calcTFarName( const Player & obj ) const
// {
//     return obj.nameTooFar();
// }

// const
// std::string &
// VisualSenderPlayerV1::calcUFarName( const Player & obj ) const
// {
//     return obj.nameFar();
// }

/*!
//===================================================================
//
//  CLASS: VisualSenderPlayerV4
//
//  DESC: Class for the version 4 visual protocol.  This version
//        introduced body directions of players. Everything else is
//        the same.
//
//===================================================================
*/

VisualSenderPlayerV4::VisualSenderPlayerV4( const Params & params )
    : VisualSenderPlayerV1( params )
{}

VisualSenderPlayerV4::~VisualSenderPlayerV4()
{}

void
VisualSenderPlayerV4::serializePlayer( const Player & player,
                                       const double & dist,
                                       const int dir,
                                       const double & dist_chg,
                                       const double & dir_chg )
{
    serializer().serializeVisualObject( transport(),
                                        calcName( player ),
                                        dist, dir, dist_chg, dir_chg,
                                        calcBodyDir( player ) );
}

// int
// VisualSenderPlayerV4::calcBodyDir( const Player & player ) const
// {
//     return rad2Deg( normalize_angle( player.angleBodyCommitted()
//                                      - self().angleBodyCommitted()
//                                      - self().angleNeckCommitted() ) );
// }

/*!
//===================================================================
//
//  CLASS: VisualSensorPlayerV5
//
//  DESC: Class for the version 5 visual protocol.  This version
//        introduced head directions of players. Everything else is
//        the same
//
//===================================================================
*/


VisualSenderPlayerV5::VisualSenderPlayerV5( const Params & params )
    : VisualSenderPlayerV4( params )
{}


VisualSenderPlayerV5::~VisualSenderPlayerV5()
{}

void
VisualSenderPlayerV5::serializePlayer( const Player & player,
                                       const double & dist,
                                       const int dir,
                                       const double & dist_chg,
                                       const double & dir_chg )
{
    serializer().serializeVisualObject( transport(),
                                        calcName( player ),
                                        dist, dir, dist_chg, dir_chg,
                                        calcBodyDir( player ),
                                        calcHeadDir( player ) );
}

/*!
//===================================================================
//
//  CLASS: VisualSensorPlayerV6
//
//  DESC: Class for the version 6 visual protocol.  This version
//        introduced shortened names for all the objects meaning the
//        name calculation must be redefined.
//
//===================================================================
*/

VisualSenderPlayerV6::VisualSenderPlayerV6( const Params & params )
    : VisualSenderPlayerV5( params )
{}

VisualSenderPlayerV6::~VisualSenderPlayerV6()
{}

/*!
//===================================================================
//
//  CLASS: VisualSensorPlayerV7
//
//  DESC: Class for the version 7 visual protocol.  This version
//        fixed a bug in the generation of directions in that they
//        were truncated to int, rather than rounded.  This meant
//        error in the direction pointed was at most times between
//        -0.5 and +0.5 degrees, but occationally between -1.0 and
//        +1.0 degrees and at other times exact.  With the new method
//        of rounding, the error is always between -0.5 and +0.5.
//
//===================================================================
*/

VisualSenderPlayerV7::VisualSenderPlayerV7( const Params& params )
    : VisualSenderPlayerV6( params )
{}

VisualSenderPlayerV7::~VisualSenderPlayerV7()
{}


/*!
//===================================================================
//
//  CLASS: VisualSensorPlayerV8
//
//  DESC: Class for the version 8 visual protocol.  This version
//        introduced observation of the a new arm feature, that
//        allows plays to point to different spots, however, on
//        direction the arm is pointing to is visable, not the
//        distance to the spot the arm is pointing to.
//
//===================================================================
*/

VisualSenderPlayerV8::VisualSenderPlayerV8( const Params& params )
    : VisualSenderPlayerV7( params )
{}

VisualSenderPlayerV8::~VisualSenderPlayerV8()
{}

int
VisualSenderPlayerV8::calcPointDir( const Player & player )
{
    double dir = 0.0;
    //if ( player.getArmDir( dir ) )
    if ( player.arm().getRelDir( rcss::geom::Vector2D( player.pos().x, player.pos().y ),
                                 player.angleBodyCommitted() + player.angleNeckCommitted(),
                                 dir ) )
    {
        dir += player.angleNeckCommitted()
            + player.angleBodyCommitted()
            - self().angleBodyCommitted()
            - self().angleNeckCommitted();

        // add random noise

        // With a normal distribution, 95% of the values are within 2
        // std deviations of the mean, so if we set sigma to 1/2 our
        // desired range, then 95% of values will be in that range.
        // NOTE: This means that 5% of the time it will be outside
        // of this range.
        double sigma = self().pos().distance( player.pos() )
            / 60.0; // this should be replaced by the line below.
        //        / ServerParam::instance().getTeamTooFarLength ();
        sigma = std::pow( sigma, 4 ); // 4 should be parameterized
        // sigma is now a range between 0 and 1.0

        double min_noise_level = Deg2Rad( 2.5 ) * 0.5;
        // 5.0 should be parameterized.  It is halved for the same
        // reason we want sigma to be half the desired range
        sigma *= M_PI - min_noise_level;
        sigma += min_noise_level;

        //sigma is now in a range of 2.5 to 180 degrees, dependant on
        //the distance of the player.  95% of the returned random values
        //will be within +- 2*sigma of dir
        boost::normal_distribution<> rng( dir, sigma );
        boost::variate_generator< rcss::random::DefaultRNG &,
            boost::normal_distribution<> >
            gen( rcss::random::DefaultRNG::instance(), rng );

        return rad2Deg( normalize_angle( gen() ) );
    }
    else
    {
        return 0;
    }
}

void
VisualSenderPlayerV8::serializePlayer( const Player & player,
                                       const std::string & name,
                                       const double & dist,
                                       const int dir,
                                       const double & dist_chg,
                                       const double & dir_chg )
{
    if ( player.arm().isPointing() )
    {
        int point_dir = calcPointDir( player );
        serializer().serializeVisualObject( transport(),
                                            name,
                                            dist, dir,
                                            dist_chg, dir_chg,
                                            calcBodyDir( player ),
                                            calcHeadDir( player ),
                                            point_dir,
                                            player.isTackling() );
    }
    else
    {
        serializer().serializeVisualObject( transport(),
                                            name,
                                            dist, dir,
                                            dist_chg, dir_chg,
                                            calcBodyDir( player ),
                                            calcHeadDir( player ),
                                            player.isTackling() );
    }
}

void
VisualSenderPlayerV8::serializePlayer( const Player & player,
                                       const std::string & name,
                                       const double & dist,
                                       const int dir )
{
    if ( player.arm().isPointing() )
    {
        int point_dir = calcPointDir( player );
        serializer().serializeVisualObject( transport(),
                                            name,
                                            dist,
                                            dir,
                                            point_dir,
                                            player.isTackling() );
    }
    else
    {
        serializer().serializeVisualObject( transport(),
                                            name,
                                            dist,
                                            dir,
                                            player.isTackling() );
    }
}


/*!
//===================================================================
//
//  CLASS: VisualSenderCoach
//
//  DESC: Base class for the visual protocol for coaches.
//
//===================================================================
*/

VisualSenderCoach::Factory &
VisualSenderCoach::factory()
{
    static Factory rval;
    return rval;
}

VisualSenderCoach::VisualSenderCoach( const Params & params )
    : VisualSender( params.M_transport ),
      M_ser( params.M_ser ),
      M_self( params.M_self ),
      M_stadium( params.M_stadium )
{

}

VisualSenderCoach::~VisualSenderCoach()
{

}

/*!
//===================================================================
//
//  CLASS: VisualSenderCoachV1
//
//  DESC:
//
//===================================================================
*/

VisualSenderCoachV1::VisualSenderCoachV1( const Params & params )
    : VisualSenderCoach( params )
{

}

VisualSenderCoachV1::~VisualSenderCoachV1()
{

}

void
VisualSenderCoachV1::sendVisual()
{
    serializer().serializeVisualBegin( transport(), stadium().time() );

    sendGoals();
    sendBall();

    const Stadium::PlayerCont::const_iterator end = stadium().players().end();
    for ( Stadium::PlayerCont::const_iterator p = stadium().players().begin();
          p != end;
          ++p )
    {
        if ( (*p)->alive() == DISABLE ) continue;

        serializePlayer( **p );
    }

    serializer().serializeVisualEnd( transport() );
    transport() << std::ends << std::flush;
}

void
VisualSenderCoachV1::sendLook()
{
    serializer().serializeLookBegin( transport(), stadium().time() );

    sendGoals();
    sendBall();

    const Stadium::PlayerCont::const_iterator end = stadium().players().end();
    for ( Stadium::PlayerCont::const_iterator p = stadium().players().begin();
          p != end;
          ++p )
    {
        if ( (*p)->alive() == DISABLE ) continue;

        serializePlayerLook( **p );
    }

    serializer().serializeLookEnd( transport() );
    transport() << std::ends << std::flush;
}

void
VisualSenderCoachV1::sendOKEye()
{
    serializer().serializeOKEye( transport(), self().isEyeOn() );
    transport() << std::ends << std::flush;
}

void
VisualSenderCoachV1::sendGoals()
{
    const std::vector< const PObject * >::const_iterator end = stadium().goals().end();
    for ( std::vector< const PObject * >::const_iterator it = stadium().goals().begin();
          it != end;
          ++it )
    {
        sendGoal( **it );
    }
}


void
VisualSenderCoachV1::sendGoal( const PObject & goal )
{
    serializer().serializeVisualObject( transport(),
                                        calcName( goal ),
                                        goal.pos() );
}

void
VisualSenderCoachV1::sendBall()
{
    serializer().serializeVisualObject( transport(),
                                        calcName( stadium().ball() ),
                                        stadium().ball().pos(),
                                        stadium().ball().vel() );
}

void
VisualSenderCoachV1::serializePlayer( const Player & player )
{
    serializer().serializeVisualObject( transport(),
                                        calcName( player ),
                                        player.pos(),
                                        player.vel(),
                                        rad2Deg( player.angleBodyCommitted() ),
                                        rad2Deg( player.angleNeckCommitted() ) );
}

void
VisualSenderCoachV1::serializePlayerLook( const Player & player )
{
    serializePlayer( player );
}

/*!
//===================================================================
//
//  CLASS: VisualSenderCoachV7
//
//  DESC:
//
//===================================================================
*/

VisualSenderCoachV7::VisualSenderCoachV7( const Params & params )
    : VisualSenderCoachV1( params )
{

}

VisualSenderCoachV7::~VisualSenderCoachV7()
{

}

/*!
//===================================================================
//
//  CLASS: VisualSenderCoachV8
//
//  DESC:
//
//===================================================================
*/

VisualSenderCoachV8::VisualSenderCoachV8( const Params & params )
    : VisualSenderCoachV7( params )
{

}

VisualSenderCoachV8::~VisualSenderCoachV8()
{

}

int
VisualSenderCoachV8::calcPointDir( const Player & player )
{
    double arm_dir = 0.0;
    if ( player.arm().getRelDir( rcss::geom::Vector2D( player.pos().x,
                                                       player.pos().y ),
                                 player.angleBodyCommitted()
                                 + player.angleNeckCommitted(),
                                 arm_dir ) )
    {
        return Rad2IDegRound( normalize_angle( arm_dir
                                               + player.angleNeckCommitted()
                                               + player.angleBodyCommitted() ) );
    }

    return 0;
}

void
VisualSenderCoachV8::serializePlayer( const Player & player )
{
    if ( player.arm().isPointing() )
    {
        serializer().serializeVisualObject( transport(),
                                            calcName( player ),
                                            player.pos(),
                                            player.vel(),
                                            rad2Deg( player.angleBodyCommitted() ),
                                            rad2Deg( player.angleNeckCommitted() ),
                                            calcPointDir( player ),
                                            player.isTackling() );
    }
    else
    {
        serializer().serializeVisualObject( transport(),
                                            calcName( player ),
                                            player.pos(),
                                            player.vel(),
                                            rad2Deg( player.angleBodyCommitted() ),
                                            rad2Deg( player.angleNeckCommitted() ),
                                            player.isTackling() );
    }
}

void
VisualSenderCoachV8::serializePlayerLook( const Player & player )
{
    VisualSenderCoachV7::serializePlayer( player );
}

/*!
//===================================================================
//
//  Register senders for different versions
//
//===================================================================
*/

namespace visual
{
template< typename Sender >
std::auto_ptr< VisualSenderPlayer >
create( const VisualSenderPlayer::Params& params )
{ return std::auto_ptr< VisualSenderPlayer >( new Sender( params ) ); }

lib::RegHolder vp1 = VisualSenderPlayer::factory().autoReg( &create< VisualSenderPlayerV1 >, 1 );
lib::RegHolder vp2 = VisualSenderPlayer::factory().autoReg( &create< VisualSenderPlayerV1 >, 2 );
lib::RegHolder vp3 = VisualSenderPlayer::factory().autoReg( &create< VisualSenderPlayerV1 >, 3 );
lib::RegHolder vp4 = VisualSenderPlayer::factory().autoReg( &create< VisualSenderPlayerV4 >, 4 );
lib::RegHolder vp5 = VisualSenderPlayer::factory().autoReg( &create< VisualSenderPlayerV5 >, 5 );
lib::RegHolder vp6 = VisualSenderPlayer::factory().autoReg( &create< VisualSenderPlayerV6 >, 6 );
lib::RegHolder vp7 = VisualSenderPlayer::factory().autoReg( &create< VisualSenderPlayerV7 >, 7 );
lib::RegHolder vp8 = VisualSenderPlayer::factory().autoReg( &create< VisualSenderPlayerV8 >, 8 );
lib::RegHolder vp9 = VisualSenderPlayer::factory().autoReg( &create< VisualSenderPlayerV8 >, 9 );
lib::RegHolder vp10 = VisualSenderPlayer::factory().autoReg( &create< VisualSenderPlayerV8 >, 10 );
lib::RegHolder vp11 = VisualSenderPlayer::factory().autoReg( &create< VisualSenderPlayerV8 >, 11 );
lib::RegHolder vp12 = VisualSenderPlayer::factory().autoReg( &create< VisualSenderPlayerV8 >, 12 );


template< typename Sender >
VisualSenderCoach::Ptr
create( const VisualSenderCoach::Params & params )
{
    return VisualSenderCoach::Ptr( new Sender( params ) );
}

lib::RegHolder vc1 = VisualSenderCoach::factory().autoReg( &create< VisualSenderCoachV1 >, 1 );
lib::RegHolder vc2 = VisualSenderCoach::factory().autoReg( &create< VisualSenderCoachV1 >, 2 );
lib::RegHolder vc3 = VisualSenderCoach::factory().autoReg( &create< VisualSenderCoachV1 >, 3 );
lib::RegHolder vc4 = VisualSenderCoach::factory().autoReg( &create< VisualSenderCoachV1 >, 4 );
lib::RegHolder vc5 = VisualSenderCoach::factory().autoReg( &create< VisualSenderCoachV1 >, 5 );
lib::RegHolder vc6 = VisualSenderCoach::factory().autoReg( &create< VisualSenderCoachV1 >, 6 );
lib::RegHolder vc7 = VisualSenderCoach::factory().autoReg( &create< VisualSenderCoachV7 >, 7 );
lib::RegHolder vc8 = VisualSenderCoach::factory().autoReg( &create< VisualSenderCoachV8 >, 8 );
lib::RegHolder vc9 = VisualSenderCoach::factory().autoReg( &create< VisualSenderCoachV8 >, 9 );
lib::RegHolder vc10 = VisualSenderCoach::factory().autoReg( &create< VisualSenderCoachV8 >, 10 );
lib::RegHolder vc11 = VisualSenderCoach::factory().autoReg( &create< VisualSenderCoachV8 >, 11 );
lib::RegHolder vc12 = VisualSenderCoach::factory().autoReg( &create< VisualSenderCoachV8 >, 12 );

}
}
