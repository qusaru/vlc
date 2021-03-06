/*
 * PersistentConnection.h
 *****************************************************************************
 * Copyright (C) 2010 - 2012 Klagenfurt University
 *
 * Created on: Aug 10, 2010
 * Authors: Christopher Mueller <christopher.mueller@itec.uni-klu.ac.at>
 *          Christian Timmerer  <christian.timmerer@itec.uni-klu.ac.at>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef PERSISTENTCONNECTION_H_
#define PERSISTENTCONNECTION_H_

#include "HTTPConnection.h"
#include <deque>

namespace adaptative
{
    namespace http
    {
        class PersistentConnection : public HTTPConnection
        {
            public:
                PersistentConnection(stream_t *stream, Chunk *chunk = NULL);

                virtual bool        connect     (const std::string &hostname, int port = 80);
                virtual bool        query       (const std::string& path);
                virtual ssize_t     read        (void *p_buffer, size_t len);
                virtual void        disconnect  ();
                virtual void        releaseChunk();

                const std::string&  getHostname () const;

            private:
                bool                queryOk;
                int                 retries;

            protected:
                static const int    retryCount = 5;
                virtual std::string buildRequestHeader(const std::string &path) const;
        };
    }
}

#endif /* PERSISTENTCONNECTION_H_ */
