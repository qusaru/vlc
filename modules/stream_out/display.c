/*****************************************************************************
 * display.c: display stream output module
 *****************************************************************************
 * Copyright (C) 2001, 2002 VideoLAN
 * $Id$
 *
 * Authors: Laurent Aimar <fenrir@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <vlc/vlc.h>
#include <vlc/input.h>
#include <vlc/sout.h>

#include "codecs.h"

/*****************************************************************************
 * Exported prototypes
 *****************************************************************************/
static int      Open    ( vlc_object_t * );
static void     Close   ( vlc_object_t * );

static sout_stream_id_t *Add ( sout_stream_t *, es_format_t * );
static int               Del ( sout_stream_t *, sout_stream_id_t * );
static int               Send( sout_stream_t *, sout_stream_id_t *, sout_buffer_t* );

/*****************************************************************************
 * Module descriptor
 *****************************************************************************/
vlc_module_begin();
    set_description( _("Display stream output") );
    set_capability( "sout stream", 50 );
    add_shortcut( "display" );
    set_callbacks( Open, Close );
vlc_module_end();

struct sout_stream_sys_t
{
    input_thread_t *p_input;

    vlc_bool_t     b_audio;
    vlc_bool_t     b_video;

    mtime_t        i_delay;
};

/*****************************************************************************
 * Open:
 *****************************************************************************/
static int Open( vlc_object_t *p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys;
    char              *val;
    p_sys           = malloc( sizeof( sout_stream_sys_t ) );
    p_sys->p_input  = vlc_object_find( p_stream, VLC_OBJECT_INPUT, FIND_ANYWHERE );
    if( !p_sys->p_input )
    {
        msg_Err( p_stream, "cannot find p_input" );
        free( p_sys );
        return VLC_EGENERIC;
    }

    p_sys->b_audio = VLC_TRUE;
    p_sys->b_video = VLC_TRUE;
    p_sys->i_delay = 100*1000;
    if( sout_cfg_find( p_stream->p_cfg, "noaudio" ) )
    {
        p_sys->b_audio = VLC_FALSE;
    }
    if( sout_cfg_find( p_stream->p_cfg, "novideo" ) )
    {
        p_sys->b_video = VLC_FALSE;
    }
    if( ( val = sout_cfg_find_value( p_stream->p_cfg, "delay" ) ) )
    {
        p_sys->i_delay = (mtime_t)atoi( val ) * (mtime_t)1000;
    }

    p_stream->pf_add    = Add;
    p_stream->pf_del    = Del;
    p_stream->pf_send   = Send;

    p_stream->p_sys     = p_sys;

    /* update p_sout->i_out_pace_nocontrol */
    p_stream->p_sout->i_out_pace_nocontrol++;

    return VLC_SUCCESS;
}

/*****************************************************************************
 * Close:
 *****************************************************************************/
static void Close( vlc_object_t * p_this )
{
    sout_stream_t     *p_stream = (sout_stream_t*)p_this;
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    /* update p_sout->i_out_pace_nocontrol */
    p_stream->p_sout->i_out_pace_nocontrol--;

    vlc_object_release( p_sys->p_input );

    free( p_sys );
}

struct sout_stream_id_t
{
    es_descriptor_t *p_es;
};

static sout_stream_id_t * Add( sout_stream_t *p_stream, es_format_t *p_fmt )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;
    sout_stream_id_t *id;

    if( ( p_fmt->i_cat == AUDIO_ES && !p_sys->b_audio )||
        ( p_fmt->i_cat == VIDEO_ES && !p_sys->b_video ) )
    {
        return NULL;
    }

    id = malloc( sizeof( sout_stream_id_t ) );

    vlc_mutex_lock( &p_sys->p_input->stream.stream_lock );
    id->p_es = input_AddES( p_sys->p_input,
                            NULL,           /* no program */
                            12,             /* es_id */
                            p_fmt->i_cat,   /* es category */
                            NULL,           /* description */
                            0 );            /* no extra data */

    if( !id->p_es )
    {
        vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );

        msg_Err( p_stream, "cannot create es" );
        free( id );
        return NULL;
    }
    id->p_es->i_stream_id   = 1;
    id->p_es->i_fourcc      = p_fmt->i_codec;
    id->p_es->b_force_decoder = VLC_TRUE;

    es_format_Copy( &id->p_es->fmt, p_fmt );

    if( input_SelectES( p_sys->p_input, id->p_es ) )
    {
        input_DelES( p_sys->p_input, id->p_es );
        vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );

        msg_Err( p_stream, "cannot select es" );
        free( id );
        return NULL;
    }
    vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );

    return id;
}

static int Del( sout_stream_t *p_stream, sout_stream_id_t *id )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    input_DelES( p_sys->p_input, id->p_es );

    free( id );

    return VLC_SUCCESS;
}

static int Send( sout_stream_t *p_stream, sout_stream_id_t *id,
                 sout_buffer_t *p_buffer )
{
    sout_stream_sys_t *p_sys = p_stream->p_sys;

    while( p_buffer )
    {
        sout_buffer_t *p_next;
        block_t *p_block;

        vlc_mutex_lock( &p_sys->p_input->stream.stream_lock );
        if( id->p_es->p_dec && p_buffer->i_size > 0 &&
            (p_block = block_New( p_stream, p_buffer->i_size )) )
        {
            p_block->i_dts = p_buffer->i_dts <= 0 ? 0 :
                             p_buffer->i_dts + p_sys->i_delay;
            p_block->i_pts = p_buffer->i_pts <= 0 ? 0 :
                             p_buffer->i_pts + p_sys->i_delay;

            p_stream->p_vlc->pf_memcpy( p_block->p_buffer,
                                        p_buffer->p_buffer, p_buffer->i_size );

            input_DecodeBlock( id->p_es->p_dec, p_block );
        }
        vlc_mutex_unlock( &p_sys->p_input->stream.stream_lock );

        /* *** go to next buffer *** */
        p_next = p_buffer->p_next;
        sout_BufferDelete( p_stream->p_sout, p_buffer );
        p_buffer = p_next;
    }

    return VLC_SUCCESS;
}
