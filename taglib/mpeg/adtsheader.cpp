/***************************************************************************
    copyright           : (C) 2018 inMusic brands, inc.
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License version   *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA         *
 *   02110-1301  USA                                                       *
 *                                                                         *
 *   Alternatively, this file is available under the Mozilla Public        *
 *   License Version 1.1.  You may obtain a copy of the License at         *
 *   http://www.mozilla.org/MPL/                                           *
 ***************************************************************************/

#include <tbytevector.h>
#include <tstring.h>
#include <tfile.h>
#include <tdebug.h>
#include <trefcounter.h>

#include "adtsheader.h"
#include "mpegutils.h"

using namespace TagLib;

class MPEG::ADTSHeader::HeaderPrivate : public RefCounter
{
public:
  HeaderPrivate() :
    isValid(false),
    version(Header::Version4),
    protectionEnabled(false),
    bitrate(0),
    sampleRate(0),
    channelMode(Custom),
    isCopyrighted(false),
    isOriginal(false),
    frameLength(0) {}

  bool isValid;
  Header::Version version;
  bool protectionEnabled;
  int bitrate;
  int sampleRate;
  ChannelMode channelMode;
  bool isCopyrighted;
  bool isOriginal;
  int frameLength;
};

////////////////////////////////////////////////////////////////////////////////
// public members
////////////////////////////////////////////////////////////////////////////////

MPEG::ADTSHeader::ADTSHeader(TagLib::File *file, long offset) :
  d(new HeaderPrivate())
{
  parse(file, offset);
}

MPEG::ADTSHeader::ADTSHeader(const ADTSHeader &h) :
  d(h.d)
{
  d->ref();
}

MPEG::ADTSHeader::~ADTSHeader()
{
  if(d->deref())
    delete d;
}

bool MPEG::ADTSHeader::isValid() const
{
  return d->isValid;
}

MPEG::Header::Version MPEG::ADTSHeader::version() const
{
  return d->version;
}

bool MPEG::ADTSHeader::protectionEnabled() const
{
  return d->protectionEnabled;
}

int MPEG::ADTSHeader::bitrate() const
{
  return d->bitrate;
}

int MPEG::ADTSHeader::sampleRate() const
{
  return d->sampleRate;
}

MPEG::ADTSHeader::ChannelMode MPEG::ADTSHeader::channelMode() const
{
  return d->channelMode;
}

bool MPEG::ADTSHeader::isCopyrighted() const
{
  return d->isCopyrighted;
}

bool MPEG::ADTSHeader::isOriginal() const
{
  return d->isOriginal;
}

int MPEG::ADTSHeader::frameLength() const
{
  return d->frameLength;
}

MPEG::ADTSHeader &MPEG::ADTSHeader::operator=(const ADTSHeader &h)
{
  if(&h == this)
    return *this;

  if(d->deref())
    delete d;

  d = h.d;
  d->ref();
  return *this;
}

////////////////////////////////////////////////////////////////////////////////
// private members
////////////////////////////////////////////////////////////////////////////////

void MPEG::ADTSHeader::parse(File *file, long offset)
{
  file->seek(offset);
  const ByteVector data = file->readBlock(6);

  if(data.size() < 4) {
    debug("MPEG::ADTSHeader::parse() -- data is too short for an MPEG frame header.");
    return;
  }

  // Check for the MPEG sync bytes.

  if(!isFrameSync(data)) {
    debug("MPEG::ADTSHeader::parse() -- MPEG header did not match MPEG synch.");
    return;
  }

  // Set the MPEG version

  if((static_cast<unsigned char>(data[1]) & 0x08) == 0x08)
    d->version = Header::Version2;
  else
    d->version = Header::Version4;

  // Set the MPEG layer

  d->protectionEnabled = (static_cast<unsigned char>(data[1] & 0x01) == 0);

  // Set the sample rate

  static const int sampleRates[] = { 96000, 88200, 64000, 48000, 44100, 32000, 24000, 22050, 16000, 12000, 11025, 8000, 7350, 0, 0, 0 };

  const int samplerateIndex = (static_cast<unsigned char>(data[2]) >> 2) & 0x0F;

  d->sampleRate = sampleRates[samplerateIndex];

  if(d->sampleRate == 0)
    return;

  // The channel mode is encoded as a 3 bit value at the end of the 3nd byte,
  // i.e. xxxxx111

  d->channelMode = static_cast<ChannelMode>((static_cast<unsigned char>(data[3]) >> 6) & 0x07);

  // TODO: Add mode extension for completeness

  d->isOriginal    = ((static_cast<unsigned char>(data[3]) & 0x04) != 0);
  d->isCopyrighted = ((static_cast<unsigned char>(data[3]) & 0x08) != 0);

  // Calculate the frame length

  if(data.size() >= 6) {
    d->frameLength = (static_cast<unsigned char>(data[3]) & 0x3) << 11 |
                     (static_cast<unsigned char>(data[4]) << 3) |
                     (static_cast<unsigned char>(data[5]) >> 5);

    d->bitrate = static_cast<int>(d->frameLength * d->sampleRate / 1024.0 + 0.5) * 8 / 1024;
  }
  // Now that we're done parsing, set this to be a valid frame.

  d->isValid = true;
}

