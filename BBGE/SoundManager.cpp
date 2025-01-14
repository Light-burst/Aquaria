/*
Copyright (C) 2007, 2010 - Bit-Blot

This file is part of Aquaria.

Aquaria is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#include "Core.h"
#include "SoundManager.h"
#include "Base.h"
#include "ttvfs_stdio.h"

#include "FmodOpenALBridge.h"



#define MAX_DYNAMIC_LAYERS 5


SoundManager *sound = 0;

std::string soundPath = "sfx/cache/";
std::string localSoundPath = "sfx/local/";
std::string musicPath = "mus/";
std::string voicePath = "vox/";
std::string fileType = ".ogg";

namespace SoundCore
{

	typedef std::map<std::string, FMOD::Sound*> SoundMap;
	SoundMap soundMap;

	FMOD_RESULT result;
	FMOD::System *system=0;

    FMOD::Sound *musicStream=0, *voiceStream=0, *tempMusic=0;
    std::vector<FMOD::Sound*> dynMusicStream;

    FMOD::Channel *musicChannel=0, *tempMusicChannel=0;
    std::vector<FMOD::Channel*> dynMusicChannel;
    FMOD::Channel *voiceChannel=0;

	FMOD::ChannelGroup *group_vox = 0;
	FMOD::ChannelGroup *group_sfx = 0;
	FMOD::ChannelGroup *group_mus = 0;

	FMOD::Sound *modSound = 0;
	FMOD::Channel *modChannel = 0;

	FMOD::ChannelGroup *masterChannelGroup = 0;

	FMOD::DSP *dspReverb=0, *dspFlange=0;

	float musicFader2Time = 0, musicFader2Timer = 0, musicFader2Volume = 0;

	struct FadeCh
	{
	public:
		FadeCh() : c(0), v(1), s(1), to(0), d(-1) {}
		FMOD::Channel *c;
		float v,s,to;
		int d;
	};

	float faderV=1;
	FMOD::Channel *faderCh = 0;


	bool stopMusicOnFadeOut=false;
	bool wasPlayingVoice=false;
	bool playingMusicOnce=false;

	typedef std::list<FadeCh> FadeChs;
	FadeChs fadeChs;

	void addFadeCh(const FadeCh &fadeCh)
	{
		FadeChs::iterator i = fadeChs.begin();
		for (; i != fadeChs.end();)
		{
			if ((*i).c == fadeCh.c)
			{
				i = fadeChs.erase(i);
			}
			else
			{
				i++;
			}
		}
		fadeChs.push_back(fadeCh);
	}
}

using namespace SoundCore;


/*
	TIPS:

	1. use F_CALLBACK.  Do NOT force cast your own function to fmod's callback type.
	2. return FMOD_ERR_FILE_NOTFOUND in open as required.
	3. return number of bytes read in read callback.  Do not get the size and count
	   around the wrong way in fread for example, this would return 1 instead of the number of bytes read.

	QUESTIONS:

	1. Why does fmod seek to the end and read?  Because it is looking for ID3V1 tags.
	   Use FMOD_IGNORETAGS in System::createSound / System::createStream if you don't like this behaviour.

*/

FMOD_RESULT F_CALLBACK myopen(const char *name, int unicode, unsigned int *filesize, void **handle, void **userdata)
{
	if (name)
	{
		VFILE *fp = vfopen(name, "rb");
		if (!fp)
			return FMOD_ERR_FILE_NOTFOUND;
		size_t sz = 0;
		if(vfsize(fp, &sz) < 0)
		{
			vfclose(fp);
			return FMOD_ERR_FILE_NOTFOUND;
		}

		*filesize = (unsigned int)sz;
		*userdata = (void *)0x12345678;
		*handle = fp;
	}

	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK myclose(void *handle, void *userdata)
{
	if (!handle)
	{
		return FMOD_ERR_INVALID_PARAM;
	}

	vfclose((VFILE *)handle);

	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK myread(void *handle, void *buffer, unsigned int sizebytes, unsigned int *bytesread, void *userdata)
{
	if (!handle)
	{
		return FMOD_ERR_INVALID_PARAM;
	}

	if (bytesread)
	{
		*bytesread = (int)vfread(buffer, 1, sizebytes, (VFILE *)handle);

		if (*bytesread < sizebytes)
		{
			return FMOD_ERR_FILE_EOF;
		}
	}

	return FMOD_OK;
}

FMOD_RESULT F_CALLBACK myseek(void *handle, unsigned int pos, void *userdata)
{
	if (!handle)
	{
		return FMOD_ERR_INVALID_PARAM;
	}

	vfseek((VFILE *)handle, pos, SEEK_SET);

	return FMOD_OK;
}


void SoundManager::pause()
{
	debugLog("SoundManager::pause");

	debugLog("mus");
	result = group_mus->setPaused(true);
	checkError();

	debugLog("vox");
	result = group_vox->setPaused(true);
	checkError();

	debugLog("sfx");
	result = group_sfx->setPaused(true);
	checkError();

	debugLog("master channel");
	result = SoundCore::masterChannelGroup->setPaused(true);
	checkError();

	debugLog("update");
	result = SoundCore::system->update();
	checkError();

	debugLog("done");
}

void SoundManager::resume()
{
	debugLog("SoundManager::resume");

	debugLog("mus");
	result = group_mus->setPaused(false);
	checkError();

	debugLog("vox");
	result = group_vox->setPaused(false);
	checkError();

	debugLog("sfx");
	result = group_sfx->setPaused(false);
	checkError();

	debugLog("master channel");
	result = SoundCore::masterChannelGroup->setPaused(false);
	checkError();

	debugLog("update");
	result = SoundCore::system->update();
	checkError();

	debugLog("done");
}

Buffer SoundManager::getBuffer(const std::string &name)
{
	std::string n = name;
	stringToLower(n);
	return soundMap[n];
}

void SoundManager::getStats(int *curAlloc, int *maxAlloc)
{
	FMOD::Memory_GetStats(curAlloc, maxAlloc);
}

SoundManager::SoundManager(const std::string &defaultDevice)
{
	overrideVoiceFader = -1;

	sound = this;

	enabled = false;

	sfxFader = 1;

	voxVol = Vector(1,1,1);
	musVol = Vector(1,1,1);
	sfxVol = 1;
	voiceFader = 1;

	loadProgressCallback = NULL;

    mode = MODE_SINGLE;
    for(int i=0;i<MAX_DYNAMIC_LAYERS;i++) {
        dynMusicStream.push_back(0);
        dynMusicChannel.push_back(0);
        dynVols.push_back(Vector(1,1,1));
    }


	int channels	= 128;

	unsigned int	 version;
	FMOD_SPEAKERMODE speakermode;
	FMOD_CAPS		caps;

	debugLog("system::create");
	result = FMOD::System_Create(&SoundCore::system);
	if (checkError()) goto get_out;

	debugLog("getVersion");
	result = SoundCore::system->getVersion(&version);
	if (checkError()) goto get_out;

	if (version < FMOD_VERSION)
	{
		char str[256];
		sprintf(str, "Error!  You are using an old version of FMOD %08x.  This program requires %08x\n", version, FMOD_VERSION);
		debugLog(str);
		goto get_out;
	}

	debugLog("driver caps");
	result = SoundCore::system->getDriverCaps(0, &caps, 0, 0, &speakermode);
	if (checkError()) goto get_out;

	debugLog("set speaker mode");
	result = SoundCore::system->setSpeakerMode(speakermode);	   /* Set the user selected speaker mode. */
	if (checkError()) goto get_out;

	debugLog("check caps");
	if (caps & FMOD_CAPS_HARDWARE_EMULATED)			 /* The user has the 'Acceleration' slider set to off!  This is really bad for latency!. */
	{												   /* You might want to warn the user about this. */
		debugLog("acceleration slider is off");
		result = SoundCore::system->setDSPBufferSize(1024, 10);	/* At 48khz, the latency between issuing an fmod command and hearing it will now be about 213ms. */
		if (checkError()) goto get_out;
	}

	debugLog("init");
	result = SoundCore::system->init(channels, FMOD_INIT_NORMAL, 0);	/* Replace with whatever channel count and flags you use! */
	if (result == FMOD_ERR_OUTPUT_CREATEBUFFER)		 /* Ok, the speaker mode selected isn't supported by this soundcard.  Switch it back to stereo... */
	{
		debugLog("err_output_createbuffer, speaker mode");
		result = SoundCore::system->setSpeakerMode(FMOD_SPEAKERMODE_STEREO);
		if (checkError()) goto get_out;

		debugLog("init 2");
		result = SoundCore::system->init(channels, FMOD_INIT_NORMAL, 0); /* Replace with whatever channel count and flags you use! */
		if (checkError()) goto get_out;
	}


	debugLog("set file system");
	result = SoundCore::system->setFileSystem(myopen, myclose, myread, myseek, 2048);
	if (checkError()) goto get_out;

	debugLog("create channel group vox");
	result = SoundCore::system->createChannelGroup("vox", &group_vox);
	if (checkError()) goto get_out;

	debugLog("create channel group sfx");
	result = SoundCore::system->createChannelGroup("sfx", &group_sfx);
	if (checkError()) goto get_out;

	debugLog("create channel group mus");
	result = SoundCore::system->createChannelGroup("mus", &group_mus);
	if (checkError()) goto get_out;

	debugLog("getMasterChannelGroup");
	result = SoundCore::system->getMasterChannelGroup(&masterChannelGroup);
	if (checkError()) goto get_out;

	debugLog("createDSPByType flange");
	result = SoundCore::system->createDSPByType(FMOD_DSP_TYPE_FLANGE, &dspFlange);
	if (checkError()) { dspFlange = 0; }

	debugLog("createDSPByType reverb");
	result = SoundCore::system->createDSPByType(FMOD_DSP_TYPE_REVERB, &dspReverb);
	if (checkError()) { dspReverb = 0; }



	if (dspReverb)
	{
		dspReverb->setParameter(FMOD_DSP_REVERB_ROOMSIZE, 0.8f);
		dspReverb->setParameter(FMOD_DSP_REVERB_DAMP, 0.7f);
		dspReverb->setParameter(FMOD_DSP_REVERB_WETMIX, 0.11f);
		dspReverb->setParameter(FMOD_DSP_REVERB_DRYMIX, 0.88f);
		dspReverb->setParameter(FMOD_DSP_REVERB_WIDTH, 1.0f);
		dspReverb->setParameter(FMOD_DSP_REVERB_MODE, 0); // 0 or 1
	}



	enabled = true;

	return;

get_out:

	debugLog("get_out");

	enabled = false;

	if (SoundCore::system)
	{
		// clean up?
		SoundCore::system = 0;
		//SoundCore::system
	}
}

void SoundManager::toggleEffectMusic(SoundEffectType effect, bool on)
{
	if (!enabled) return;


	bool active = false;

	switch(effect){
	case SFX_FLANGE:
		if (dspFlange){
			dspFlange->getActive(&active);
			if (on && !active)
				group_mus->addDSP(dspFlange, 0);
			else if (!on && active)
				dspFlange->remove();
		}
	break;
	case SFX_NONE:
	case SFX_MAX:
		break;
	}

}


std::string SoundManager::getVolumeString()
{
	std::ostringstream os;
	os << "sfxFader: " << this->sfxFader << " sfxVol: " << this->sfxVol << " [" << lastMusic << "]" << std::endl;
	os << "musVol: " << musVol.y << " voxVol: " << voxVol.y << std::endl;

	float musicChannelVol = -1;
	if (musicChannel)
		musicChannel->getVolume(&musicChannelVol);

	float musicGroupVol = -1;
	if (group_mus)
		group_mus->getVolume(&musicGroupVol);

    float tempMusicChannelVol = -1;
    if (tempMusicChannel)
        tempMusicChannel->getVolume(&tempMusicChannelVol);

    os << "curMusVol (c1/c2/g): " << musicChannelVol << " " << tempMusicChannelVol << " " << musicGroupVol << std::endl;
    return os.str();
}

float SoundManager::getMusicFader()
{
	return musVol.y;
}

float SoundManager::getVoxFader()
{
	return voxVol.y;
}

void SoundManager::setChannelVolume(int channel, float v)
{
    if(channel == 0) {
        musVol.y = v;
    }
    else {
        if(dynVols[channel-1].y > 0) {
            dynVols[channel-1].y = v;
        }
    }
}

void SoundManager::setOverrideVoiceFader(float v)
{
	overrideVoiceFader = v;
}

void SoundManager::setMusicFader(float v, float t, int channel)
{
    // ignore fades if the music is already on its way to fading out to 0
    if (v != 0 && musVol.data && musVol.data->target.y == 0 && musVol.y > 0) {
        return;
    }


    if (channel == 0) {
        musVol.interpolateTo(Vector(musVol.x, v, musVol.z), t);
    } else {
        dynVols[channel - 1].interpolateTo(Vector(dynVols[channel - 1].x, v, dynVols[channel - 1].z), t);
    }
}

void SoundManager::error(const std::string &errMsg)
{

	errorLog(errMsg);
}

SoundManager::~SoundManager()
{
	// release
	if (!enabled) return;

	for (SoundMap::iterator i = soundMap.begin(); i != soundMap.end(); i++)
	{
		std::string snd = (*i).first;
		debugLog("unloading sound [" + snd + "]");
#ifndef BBGE_DISABLE_SOUND_CACHE
		FMOD::Sound *samp = (FMOD::Sound*)((*i).second);
		samp->release();
#else
		SoundInfo *info = (SoundInfo*)((*i).second);
		delete info;
#endif
	}
	soundMap.clear();

	SoundCore::system->release();
}

void SoundManager::stopAllSfx()
{
	if (group_sfx)
		group_sfx->stop();
}

void SoundManager::stopAll()
{
}

void SoundManager::onVoiceEnded()
{
	event_stopVoice.call();

	if (dspReverb)
		dspReverb->remove();

	if (!voxQueue.empty())
	{
		std::string vox = voxQueue.front();
		voxQueue.pop_front();
		playVoice(vox, SVT_INTERRUPT);
	}
	else
	{
		setMusicFader(1, 1);
		sfxFader = 1;
	}
}


bool SoundManager::isPaused()
{
	bool paused = false;

	if (!enabled) return paused;


	result = masterChannelGroup->getPaused(&paused);
	checkError();


	return paused;
}

void SoundManager::clearFadingSfx()
{

	SoundCore::FadeChs::iterator i = fadeChs.begin();
	for (; i != fadeChs.end(); i++)
	{
		//haha:
		FadeCh *f = &(*i);
		if (f->c)
		{
			f->c->stop();
		}
	}
	SoundCore::fadeChs.clear();


}

void SoundManager::update(float dt)
{
	if (isPaused()) return;

	dt = core->get_old_dt();

	voxVol.update(dt);
	musVol.update(dt);
    for(int i=0; i<dynVols.size(); i++) {
        dynVols[i].update(dt);
    }



	if (musicChannel)
	{
		if (!isPlayingMusic())
		{
			if(!playingMusicOnce && lastMusic.size())
            {
                debugLog("music not playing, but it should be - force restart");
                if (mode == MODE_SINGLE) {
                    playMusic(lastMusic, SLT_LOOP, SFT_IN, 1,
                              SCT_NORMAL);
                }
                if (mode == MODE_LAYERED  ||  mode == MODE_SWITCH) {
                    std::vector<std::string> names;
                    names.push_back(lastMusic);
                    for (std::string song : lastDynMusic) {
                        names.push_back(song);
                    }
                    playDynamicMusic(names, SLT_LOOP, SFT_IN, 1, SCT_NORMAL);
                    if(mode==MODE_SWITCH) {
                        playSwitch(swtch);
                    }
                }
            }
			else
			{
				stopMusic();
			}
		}
	}

	if (musicChannel)
	{
		// fader value

		result = musicChannel->setVolume(musVol.y*1.0f);
		checkError();


		if (musVol.y <= 0 && stopMusicOnFadeOut)
		{
			stopMusic();
			stopMusicOnFadeOut = false;
		}
	}
    if(dynMusicChannel[0]) {
        for(int i=0; i<dynMusicChannel.size(); i++) {
            result = dynMusicChannel[i]->setVolume(dynVols[i].y*1.0f);
            //debugLog("dynVolume["+std::to_string(i)+"] updated to " + std::to_string(dynVols[i].y));
            //checkError();
        }
    }

	if (group_sfx)
	{
		group_sfx->setVolume(sfxFader*sfxVol);
	}


    // for cross fading
    if (tempMusicChannel)
    {
        musicFader2Timer -= dt;
        if (musicFader2Timer < 0) musicFader2Timer = 0;

        tempMusicChannel->setVolume((musicFader2Timer/musicFader2Time)*musicFader2Volume);

        if (musicFader2Timer <= 0)
        {
            result = tempMusicChannel->stop();
            checkError();

            result = tempMusic->release();
            checkError();

            tempMusicChannel = 0;
            tempMusic = 0;
        }
    }

	if (!fadeChs.empty())
	{
		int itr=0;
		for (FadeChs::iterator i = fadeChs.begin(); i != fadeChs.end();)
		{
			itr++;
			FadeCh *f = &(*i);

			f->v += dt*f->s * f->d;


			if (f->d > 0)
			{
				if (f->v >= f->to)
				{
					f->v = f->to;
					i = fadeChs.erase(i);
					continue;
				}
			}
			else
			{
				if (f->v <= f->to)
				{
					f->v = f->to;

					result = f->c->stop();
					checkError();
					f->c = 0;
					i = fadeChs.erase(i);
					continue;
				}
			}

			if (f->c)
			{
				f->c->setVolume(f->v);
				checkError();
			}

			i++;
		}
	}

	SoundCore::system->update();


	if (wasPlayingVoice && !isPlayingVoice())
	{
		wasPlayingVoice = false;
		onVoiceEnded();
	}
}

void SoundManager::fadeMusic(SoundFadeType sft, float t)
{
	switch(sft)
	{
	case SFT_CROSS:
	{
        if (tempMusicChannel)
        {
            tempMusicChannel->stop();
            if (tempMusic)
            {
                tempMusic->release();
                tempMusic = 0;
            }
            tempMusicChannel = 0;
        }

        tempMusicChannel = musicChannel;
        tempMusic = musicStream;
        musicStream = 0;
        musicChannel = 0;
        musicFader2Volume = musVol.y;
        musicFader2Time = musicFader2Timer = t;


	}
	break;
	case SFT_OUT:
		setMusicFader(0, t);

		stopMusicOnFadeOut = true;
	break;
	default:
		//setMusMul(0, t);
	break;
	}
}

bool SoundManager::isPlayingMusic()
{

	if (musicChannel)
	{
		bool b=false;
		musicChannel->isPlaying(&b);
		return b;
	}



	return false;
}

void SoundManager::setMusicVolume(float v)
{
	musVol.x = v;


	result = group_mus->setVolume(v);
	checkError();

}

void SoundManager::setSfxVolume(float v)
{
	sfxVol = v;
}

float SoundManager::getSfxVol()
{
	return sfxVol;
}

void SoundManager::setVoiceVolume(float v)
{
	voxVol.x = v;

	result = group_vox->setVolume(v);
	checkError();
}

bool SoundManager::isPlayingVoice()
{

	if (voiceChannel)
	{
		bool b=false;
		result = voiceChannel->isPlaying(&b);
		if (result == FMOD_ERR_CHANNEL_STOLEN)
		{
			b = false;
			debugLog("voice channel 'stolen'");
		}
		else
		{
			checkError();
		}
		if (!b)
			voiceChannel = 0;
		return b;
	}


	return false;
}

void SoundManager::setSfxChannelsVolume(float v)
{
}

bool SoundManager::playVoice(const std::string &name, SoundVoiceType svt, float vmod)
{
	if (!enabled) return false;

	bool checkOther = true;
	std::string fn, n;

	n = name;
	stringToLower(n);

	if (!voicePath2.empty())
	{
		fn = voicePath2 + name + fileType;
		fn = localisePathInternalModpath(fn);
		fn = adjustFilenameCase(fn);
		if (exists(fn))	checkOther = false;
	}

	if (checkOther)
	{
		fn = voicePath + name + fileType;
		fn = localisePath(fn);
		fn = adjustFilenameCase(fn);
		if (!exists(fn))
		{
			debugLog("Could not find voice file [" + fn + "]");
		}
	}

	bool playNow=false;

	debugLog("checking is playing now...");

	switch(svt)
	{
	case SVT_QUEUE:
	{
		if (isPlayingVoice())
		{
			if (voxQueue.empty() || voxQueue.front() != n)
				voxQueue.push_back(n);
		}
		else
		{
			playNow = true;
		}
	}
	break;
	case SVT_INTERRUPT:
	{
		stopAllVoice();
		playNow = true;
	}
	break;
	default:
	break;
	}

	if (playNow)
	{
		if (voiceStream)
		{
			stopVoice();
		}

		debugLog("play now");

		if (overrideVoiceFader != -1)
		{
			if (overrideVoiceFader < 1)
			{
				setMusicFader(overrideVoiceFader, 0.5);
				sfxFader = overrideVoiceFader;
			}
		}
		else
		{
			if (voiceFader < 1 )
			{
				setMusicFader(voiceFader, 0.5);
				sfxFader = voiceFader;
			}
		}



		// FMOD_DEFAULT uses the defaults.  These are the same as FMOD_LOOP_OFF | FMOD_2D | FMOD_HARDWARE.

		FMOD_MODE mode=0;
		mode = FMOD_2D | FMOD_SOFTWARE | FMOD_CREATESTREAM;

		result = SoundCore::system->createStream(fn.c_str(), mode, 0, &voiceStream);
		if (checkError())
		{
			voiceStream = 0;
		}

		if (voiceStream)
		{

			if (!reverbKeyword.empty())
			{
				bool useReverb = (n.find(reverbKeyword) != std::string::npos);

				if (dspReverb)
				{
					bool active = false;

					result = dspReverb->getActive(&active);
					checkError();

					if (!active && useReverb)
					{
						result = group_vox->addDSP(dspReverb, 0);
						checkError();
					}
					else if (active && !useReverb)
					{
						result = dspReverb->remove();
						checkError();
					}

					// defaults:
					//dspReverb->setParameter(FMOD_DSP_REVERB_ROOMSIZE, 0.5);
					//dspReverb->setParameter(FMOD_DSP_REVERB_DAMP, 0.5);
					//dspReverb->setParameter(FMOD_DSP_REVERB_WETMIX, 0.33);
					//dspReverb->setParameter(FMOD_DSP_REVERB_DRYMIX, 0.66);
					//dspReverb->setParameter(FMOD_DSP_REVERB_WIDTH, 1.0);
					//dspReverb->setParameter(FMOD_DSP_REVERB_MODE, 0); // 0 or 1
				}
			}

			result = SoundCore::system->playSound(FMOD_CHANNEL_FREE, voiceStream, true, &voiceChannel);
			checkError();

			result = voiceChannel->setChannelGroup(group_vox);
			checkError();

			if (vmod != -1)
			{
				result = voiceChannel->setVolume(vmod);
				checkError();
			}

			result = voiceChannel->setPriority(1);
			checkError();



			voiceChannel->setFrequency(1);
			voiceChannel->setCallback(NULL);
			voiceChannel->setUserData(NULL);
			voiceChannel->set3DMinMaxDistance(0.0f, 0.0f);
			setSoundRelative(voiceChannel, true);
			setSoundPos(voiceChannel, 0, 0);

			result = voiceChannel->setPaused(false);
			checkError();

			wasPlayingVoice = true;
		}


		lastVoice = n;
		event_playVoice.call();
		core->onPlayedVoice(n);
	}

	return true;
}

void SoundManager::updateChannelVolume(void *ch, float v)
{
}

float SoundManager::getVoiceTime()
{

	if (isPlayingVoice())
	{
		unsigned int position;
		voiceChannel->getPosition(&position, FMOD_TIMEUNIT_MS);
		return float(position) * 0.001f;
	}


	return 0;
}

void *SoundManager::playSfx(const PlaySfx &play)
{
	if (!enabled) return 0;


	FMOD::Channel *channel = 0;
	FMOD::Sound *sound = 0;

	if (!play.name.empty())
		sound = (FMOD::Sound*)getBuffer(play.name);

	if (!sound) return 0;

	result = SoundCore::system->playSound(FMOD_CHANNEL_FREE, sound, true, &channel);
	checkError();

	if (channel == NULL)  // the OpenAL bridge code might return NULL here.
		return 0;

	result = channel->setChannelGroup(group_sfx);
	checkError();

	result = channel->setPriority((int) ((1-play.priority)*255));
	checkError();

	if (play.fade == SFT_IN)
	{
		result = channel->setVolume(0);
		checkError();

		FadeCh fade;
		fade.c = channel;
		fade.v = 0;
		fade.s = 1.0f/play.time;
		fade.d = 1;
		fade.to = play.vol;

		SoundCore::addFadeCh(fade);
	}
	else
	{
		result = channel->setVolume(play.vol);
		checkError();
	}

	float freq = play.freq;
	if (freq <= 0)
		freq = 1;
	channel->setFrequency(freq);

	channel->setCallback(NULL);
	channel->setUserData(NULL);

	// distance gain attenuation: stereo separation + silence at further away than maxdist
	float maxdist = play.maxdist;
	if (!maxdist)
		maxdist = 1800;

	if(maxdist > 0 && play.positional)
		channel->set3DMinMaxDistance(maxdist * 0.3, maxdist); // HACK: this works reasonably well
	else
		channel->set3DMinMaxDistance(0, 0); // no attenuation

	// position in space
	setSoundRelative(channel, play.relative);
	setSoundPos(channel, play.x, play.y); // must be set after everything else (See hack in OpenALChannel::set3DAttributes())



	result = channel->setPaused(false);
	checkError();

	return channel;


	return 0;
}

void *SoundManager::playSfx(const std::string &name, float vol)
{
	PlaySfx play;
	play.name = name;
	play.vol = vol;
	return playSfx(play);
}

void SoundManager::setVoiceFader(float v)
{
	voiceFader = v;
}

bool SoundManager::isPlayingMusic(const std::string &name)
{
	if (isPlayingMusic())
	{
		std::string test = name;
		stringToLower(test);



		if (test == lastMusic)
			return true;
	}

	return false;
}

bool SoundManager::playMusic(const std::string &name, SoundLoopType slt, SoundFadeType sft, float trans, SoundConditionType sct)
{
	debugLog("playMusic: " + name);

	if (!enabled) return false;

    if(mode == MODE_LAYERED || mode == MODE_SWITCH) {
        for(int i=0;i<dynMusicChannel.size();i++) {
            dynMusicChannel[i]->stop();
            dynMusicChannel[i] = 0;
        }
        for(int i=0;i<dynMusicStream.size();i++) {
            dynMusicStream[i]->release();
            dynMusicStream[i] = 0;
        }
        dynMusicLayers = 0;
        mode = MODE_SINGLE;
    }

	if (sct == SCT_ISNOTPLAYING && isPlayingMusic())
	{
		if (isPlayingMusic(name))
		{
			return false;
		}
	}

	std::string fn = "";
	if (!name.empty() && name[0] == '.')
	{
		fn = name;
	}
	else
	{
		if (!audioPath2.empty())
		{
			fn = audioPath2 + name + fileType;
			if (!exists(fn))
			{
				fn = musicPath + name + fileType;
			}
		}
		else
		{
			fn = musicPath + name + fileType;
		}
	}

	fn = adjustFilenameCase(fn);

	lastMusic = name;
	stringToLower(lastMusic);


	if (sft == SFT_CROSS)
	{
		fadeMusic(SFT_CROSS, trans);
	}

    if (musicStream) {
        musicStream->release();
        musicStream = 0;
    }

    if (musicChannel) {
        if (sft == SFT_IN){
            musicChannel->stop();  // we're fading in music but didn't stop the old one?
        }
        musicChannel = 0;
    }

	// FMOD_DEFAULT uses the defaults.  These are the same as FMOD_LOOP_OFF | FMOD_2D | FMOD_HARDWARE.

	FMOD_MODE mode=0;

	///FMOD_DEFAULT;////mode = FMOD_2D | FMOD_SOFTWARE;

	mode = FMOD_2D | FMOD_SOFTWARE | FMOD_CREATESTREAM;


	switch(slt)
	{
	case SLT_OFF:
	case SLT_NONE:
		mode |= FMOD_LOOP_OFF;
		playingMusicOnce = true;
	break;
	default:
		mode |= FMOD_LOOP_NORMAL;
		playingMusicOnce = false;
	break;
	}

	stopMusicOnFadeOut = false;
	musVol.stop();

	result = SoundCore::system->createStream(fn.c_str(), mode, 0, &musicStream);
	if (checkError()) musicStream = 0;

	if (musicStream)
	{
		result = SoundCore::system->playSound(FMOD_CHANNEL_FREE, musicStream, true, &musicChannel);
		checkError();

		result = musicChannel->setChannelGroup(group_mus);
		checkError();

		result = musicChannel->setPriority(0); // should be highest priority (according to the docs)
		checkError();

		if (sft == SFT_IN || sft == SFT_CROSS)
		{
			setMusicFader(0);
			setMusicFader(1,trans);

			result = musicChannel->setVolume(0);
			checkError();
		}
		else
		{
			setMusicFader(1,0);

			result = musicChannel->setVolume(musVol.y);
			checkError();
		}

		musicChannel->setFrequency(1); // in case the channel was used by a pitch-shifted sound before
		musicChannel->setCallback(NULL);
		musicChannel->setUserData(NULL);
		musicChannel->set3DMinMaxDistance(0.0f, 0.0f); // disable attenuation
		setSoundRelative(musicChannel, true);
		setSoundPos(musicChannel, 0, 0);

		result = musicChannel->setPaused(false);		// This is where the sound really starts.
		checkError();
		debugLog("music play: " + fn);
	}
	else
	{
		debugLog("Failed to create music stream: " + fn);
	}

	return true;
}


bool SoundManager::playDynamicMusic(std::vector<std::string> names, SoundLoopType slt, SoundFadeType sft, float trans, SoundConditionType sct)
{
    debugLog("playDynamicMusic: ");

    if(mode == MODE_SINGLE) {
        mode = MODE_LAYERED;
    }
    if(mode == MODE_SWITCH) {
        // Go from switch to layered
        mode = MODE_LAYERED;
    }

    for(std::string name : names) {
        debugLog(name);
    }

    if (!enabled) return false;


    if (sct == SCT_ISNOTPLAYING && isPlayingMusic()) {
        if (isPlayingMusic(names[0])) {
            return false;
        }
    }

    std::vector<std::string> fileNames;
    std::string fileName = "";
    for(std::string name : names) {
        int i=0;
        fileName = "";
        if (!name.empty() && name[0] == '.') {
            fileName = name;
        }
        else {
            if (!audioPath2.empty()) {
                fileName = audioPath2 + name + fileType;
                if (!exists(fileName)) {
                    fileName = musicPath + name + fileType;
                }
            }
            else {
                fileName = musicPath + name + fileType;
            }
        }

        fileName = adjustFilenameCase(fileName);
        fileNames.push_back(fileName);
        i++;
    }
    debugLog("fadeMusic ended");


    lastMusic = names[0];
    stringToLower(lastMusic);
    lastDynMusic.clear();
    int i=0;
    for(std::string name : names) {
        if(i==0) {
            lastMusic = name;
            stringToLower(lastMusic);
        }
        else {
            lastDynMusic.push_back(name);
            stringToLower(lastDynMusic[i-1]);
        }
        i++;
    }


    if (sft == SFT_CROSS) {
        fadeMusic(SFT_CROSS, trans);
    }

    if (tempMusicChannel)
    {
        tempMusicChannel->stop();
        if (tempMusic)
        {
            tempMusic->release();
            tempMusic = 0;
        }
        tempMusicChannel = 0;
    }

    if (musicStream) {
        musicStream->release();
        musicStream = 0;
        if(dynMusicStream[0]) {
            for(int i=0;i<dynMusicStream.size();i++) {
                dynMusicStream[i]->release();
                dynMusicStream[i] = 0;
            }
        }
    }
    debugLog("musicStream Release");

    if (musicChannel) {
        if (sft == SFT_IN){
            musicChannel->stop();  // we're fading in music but didn't stop the old one?
            if(dynMusicChannel[0]) {
                for(int i=0;i<dynMusicChannel.size();i++) {
                    dynMusicChannel[i]->stop();
                }
            }
        }
        musicChannel = 0;
        if(dynMusicChannel[0]) {
            for(int i=0;i<dynMusicChannel.size();i++) {
                dynMusicChannel[i] = 0;
            }
        }
    }
    debugLog("musicChannel Release");

    // FMOD_DEFAULT uses the defaults.  These are the same as FMOD_LOOP_OFF | FMOD_2D | FMOD_HARDWARE.

    FMOD_MODE mode=0;

    ///FMOD_DEFAULT;////mode = FMOD_2D | FMOD_SOFTWARE;

    mode = FMOD_2D | FMOD_SOFTWARE | FMOD_CREATESTREAM;


    switch(slt)
    {
        case SLT_OFF:
        case SLT_NONE:
            mode |= FMOD_LOOP_OFF;
            playingMusicOnce = true;
            break;
        default:
            mode |= FMOD_LOOP_NORMAL;
            playingMusicOnce = false;
            break;
    }

    stopMusicOnFadeOut = false;
    musVol.stop();

    for(int i=0;i<fileNames.size();i++) {
        if(i==0) {
            result = SoundCore::system->createStream(fileNames[i].c_str(), mode, 0, &musicStream);
            debugLog("main stream create");
        }
        else {
            result = SoundCore::system->createStream(fileNames[i].c_str(), mode, 0, &dynMusicStream[i-1]);
            debugLog("extra stream");
        }
    }
    if (checkError()) {
        debugLog("ERROR: Stream creation");
        musicStream = 0;
        for(int i=0;i<dynMusicStream.size();i++) {
            dynMusicStream[i] = 0;
        }
    }

    if (musicStream)
    {
        result = SoundCore::system->playSound(FMOD_CHANNEL_FREE, musicStream, true, &musicChannel);
        for(int i=0;i<dynMusicStream.size();i++) {
            if (dynMusicStream[i])  {
                result = SoundCore::system->playSound(FMOD_CHANNEL_FREE, dynMusicStream[i], true, &dynMusicChannel[i]);
            }
        }
        checkError();

        result = musicChannel->setChannelGroup(group_mus);
        for(int i=0;i<dynMusicStream.size();i++) {
            result = dynMusicChannel[i]->setChannelGroup(group_mus);
        }
        checkError();

        result = musicChannel->setPriority(0); // should be highest priority (according to the docs)
        for(int i=0;i<dynMusicStream.size();i++) {
            result = dynMusicChannel[i]->setPriority(0);
        }
        checkError();

        if (sft == SFT_IN || sft == SFT_CROSS) {
            setMusicFader(0);
            setMusicFader(1,trans);

            result = musicChannel->setVolume(0);
            checkError();
        }
        else {
            setMusicFader(1,0);
            result = musicChannel->setVolume(musVol.y);
            checkError();
        }
        debugLog("playSound");

        musicChannel->setFrequency(1); // in case the channel was used by a pitch-shifted sound before
        musicChannel->setCallback(NULL);
        musicChannel->setUserData(NULL);
        musicChannel->set3DMinMaxDistance(0.0f, 0.0f); // disable attenuation
        setSoundRelative(musicChannel, true);
        setSoundPos(musicChannel, 0, 0);
        for(int i=0;i<dynMusicStream.size();i++) {
            dynMusicChannel[i]->setFrequency(1);
            dynMusicChannel[i]->setCallback(NULL);
            dynMusicChannel[i]->setUserData(NULL);
            dynMusicChannel[i]->set3DMinMaxDistance(0.0f, 0.0f);
            setSoundRelative(dynMusicChannel[i], true);
            setSoundPos(dynMusicChannel[i], 0, 0);
        }
        debugLog("play setup");

        dynMusicLayers = names.size();
        result = musicChannel->setPaused(false);		// This is where the sound really starts.
        for(int i=0;i<dynMusicStream.size();i++) {
            dynMusicChannel[i]->setPaused(false);
        }
        checkError();
        debugLog("music play: " + fileName);

        for(int i=0;i<dynMusicStream.size();i++) {
            dynVols[i].y = 0;
            dynMusicChannel[i]->setVolume(0);
        }
        dynLevel = 1;
    }
    else {
        debugLog("Failed to create music stream: " + fileName);
    }

    return true;
}


bool SoundManager::playSwitch(bool choice)
{
    if(mode == MODE_SINGLE) { return false; }
    if(mode == MODE_LAYERED) {
        for(int i=0; i<dynMusicChannel.size(); i++) {
            setMusicFader(0, 1, i+1);
            debugLog("setVol channel[" + std::to_string(i+1) + "] down");
        }
        mode = MODE_SWITCH;
    }

    if(choice) {
        swtch = 1;
        setMusicFader(1, 1, 2);
        debugLog("setVol channel[" + std::to_string(2) + "] up");
    }
    else {
        swtch = 0;
        setMusicFader(1, 1, 1);
        debugLog("setVol channel[" + std::to_string(1) + "] up");
    }
}


bool SoundManager::playSwitch()
{
    if(mode != MODE_SWITCH) { return false; }

    if(swtch==0) {
        setMusicFader(0, 1, 1);
        setMusicFader(1, 1, 2);
        swtch = 1;
    }
    else {
        setMusicFader(0, 1, 2);
        setMusicFader(1, 1, 1);
        swtch = 0;
    }
}


bool SoundManager::stressDynamicMusic(int amount)
{
    if(mode != MODE_LAYERED) { return false; }

    int oldDynLevel = dynLevel;
    if(amount <= 0 || dynMusicLayers==0)  { return false; }
    if(amount+dynLevel > dynMusicLayers)  { dynLevel = dynMusicLayers; }
    else                                  { dynLevel += amount; }

    for(int i=oldDynLevel-1; i<dynLevel-1; i++) {
        //dynMusicChannel[i]->setVolume(1);
        setMusicFader(1, 1, i+1);
        debugLog("setVol channel[" + std::to_string(i) + "] active");
    }

    if(checkError()) { return false; }
    return true;
}

bool SoundManager::relaxDynamicMusic(int amount)
{
    if(mode != MODE_LAYERED) { return false; }

    int oldDynLevel = dynLevel;
    if(amount <= 0 || dynMusicLayers==0) { return false; }
    if(dynLevel-amount <= 0)             { dynLevel = 1; }
    else                                 { dynLevel -= amount; }

    for(int i=oldDynLevel-2; i>=dynLevel-1; i--) {
        //dynMusicChannel[i]->setVolume(0);
        setMusicFader(0, 1, i+1);
        debugLog("setVol channel[" + std::to_string(i) + "] down");
    }

    if(checkError()) { return false; }
    return true;
}


void SoundManager::stopMusic()
{
    dynMusicLayers = 0;
	if (musicChannel)
	{
		musicChannel->stop();
		checkError();
        if(dynMusicChannel[0]) {
            for(int i=0;i<dynMusicChannel.size();i++) {
                dynMusicChannel[i]->stop();
                dynMusicChannel[i] = 0;
            }
        }
		if (musicStream)
		{
			musicStream->release();
			checkError();
            if(dynMusicStream[0]) {
                for(int i=0;i<dynMusicStream.size();i++) {
                    dynMusicStream[i]->release();
                    dynMusicStream[i] = 0;
                }
            }
		}
		musicStream = 0;
		musicChannel = 0;
	}
	playingMusicOnce = false;
	lastMusic = "";
}

void SoundManager::stopSfx(void *channel)
{
	if (!channel) return;
	FMOD::Channel *ch = (FMOD::Channel*)channel;
	if (ch)
	{
		ch->stop();
		checkError();
		ch = 0;
	}
}

void SoundManager::fadeSfx(void *channel, SoundFadeType sft, float t)
{
	if (!channel) return;
	if (sft == SFT_OUT)
	{
		FMOD::Channel *ch = (FMOD::Channel*)channel;
		if (ch)
		{
			FadeCh f;
			f.s = 1.0f/t;
			result = ch->getVolume(&f.v);
			checkError();

			f.c = ch;
			SoundCore::addFadeCh(f);
		}
	}
}

void SoundManager::stopVoice()
{

	if (voiceChannel)
	{
		bool playing = false;
		result = voiceChannel->isPlaying(&playing);
		checkError();

		if (playing)
		{
			result = voiceChannel->stop();
			checkError();
			voiceChannel = 0;
		}
	}
	if (voiceStream)
	{
		result = voiceStream->release();
		checkError();
		voiceStream = 0;
	}
	onVoiceEnded();

}

void SoundManager::stopAllVoice()
{
	voxQueue.clear();
	stopVoice();
}

void loadCacheSoundsCallback (const std::string &filename, void *param)
{
	SoundManager *sm;
	sm = (SoundManager*)param;
	if (!sm->enabled)
	{

		debugLog("Disabled: Won't Load Sample ["+filename+"]");
		return;
	}
	if (fileType==".ogg")
	{
		debugLog("trying to load sound " + filename);
		sm->loadSoundIntoBank(filename, "", "");
	}
}

void SoundManager::loadSoundCache(const std::string &path, const std::string &ftype, void progressCallback())
{
	loadProgressCallback = progressCallback;
	forEachFile(path, ftype, loadCacheSoundsCallback, this);
	loadProgressCallback = NULL;
}

Buffer SoundManager::loadSoundIntoBank(const std::string &filename, const std::string &path, const std::string &format, SoundLoadType slt)
{
	if (loadProgressCallback)
		loadProgressCallback();

	std::string f = filename, name;

	// HACK: proper sound looping
	bool loop = false;
	stringToLower(f);
	if (f.find("loop")!=std::string::npos)
		loop = true;

	// WARNING: local sounds should go here!

	debugLog(filename);
	if (slt == SFXLOAD_LOCAL && !audioPath2.empty())
	{
		f = audioPath2 + filename + format;
		f = localisePathInternalModpath(f);
		f = adjustFilenameCase(f);
		if (!exists(f))
		{
			f = path + filename + format;
			f = localisePath(f);
			f = adjustFilenameCase(f);
		}
	}
	else
	{
		f = path + filename + format;
		f = localisePath(f);
		f = adjustFilenameCase(f);
	}

	size_t loc = f.find_last_of('/');
	size_t loc2 = f.rfind('.');
	if (loc != std::string::npos && loc2 != std::string::npos)
	{
		name = f.substr(loc+1, loc2-(loc+1));
	}
	else
	{
		debugLog("returning 0");
		return Buffer();
	}

	stringToLower(name);


	FMOD::Sound * sound = SoundCore::soundMap[name];

	if (sound)
		return sound;

	FMOD_MODE mode = FMOD_DEFAULT | FMOD_LOWMEM | FMOD_3D | FMOD_3D_LINEARROLLOFF;
	if (loop)
		mode |= FMOD_LOOP_NORMAL;

	result = SoundCore::system->createSound(f.c_str(), mode, 0, &sound);
	if (checkError())
	{
		debugLog("createSound failed");
		return Buffer();
	}

	SoundCore::soundMap[name] = sound;


	if (slt == SFXLOAD_LOCAL)
	{
		localSounds.push_back(name);
	}

	return sound;



	return Buffer();
}

Buffer SoundManager::loadLocalSound(const std::string &filename)
{
	return loadSoundIntoBank(filename, localSoundPath, fileType, SFXLOAD_LOCAL);
}

void SoundManager::setMusicSpeed(float speed)
{
	musicChannel->setFrequency(speed);
}

void SoundManager::setModSpeed(float speed)
{
	if (modChannel)
		modChannel->setFrequency(speed);
}

void SoundManager::clearLocalSounds()
{
	for (LocalSounds::iterator i = localSounds.begin(); i != localSounds.end(); i++)
	{
		std::string snd = (*i);
		debugLog("unloading sound [" + snd + "]");
		FMOD::Sound *samp = (FMOD::Sound*)soundMap[snd];
		samp->release();
		soundMap[snd] = 0;
	}
	localSounds.clear();
}

bool SoundManager::checkError()
{
	if (result != FMOD_OK)
	{
		std::ostringstream os;
		os << "FMODEX error: " << result << ": ";

		switch(result)
		{
		case FMOD_ERR_ALREADYLOCKED:
			os << "FMOD_ERR_ALREADYLOCKED Tried to call lock a second time before unlock was called.";
		break;
		case FMOD_ERR_BADCOMMAND:
			os << "Tried to call a function on a data type that does not allow this type of functionality (ie calling Sound::lock on a streaming sound).";
		break;
		case FMOD_ERR_CHANNEL_ALLOC:
			os << "FMOD_ERR_CHANNEL_ALLOC: Error trying to allocate a channel.";
		break;
		case FMOD_ERR_CHANNEL_STOLEN:
			os << "FMOD_ERR_CHANNEL_STOLEN: The specified channel has been reused to play another sound.";
		break;
		case FMOD_ERR_COM:
			os << "FMOD_ERR_COM: A Win32 COM related error occured. COM failed to initialize or a QueryInterface failed meaning a Windows codec or driver was not installed properly.";
		break;
		case FMOD_ERR_DMA:
			os << "FMOD_ERR_DMA: DMA Failure. See debug output for more information.";
		break;
		case FMOD_ERR_DSP_CONNECTION:
			os << "FMOD_ERR_DSP_CONNECTION: DSP connection error. Connection possibly caused a cyclic dependancy.";
		break;
		case FMOD_ERR_DSP_FORMAT:
			os << "FMOD_ERR_DSP_FORMAT: DSP Format error. A DSP unit may have attempted to connect to this network with the wrong format.";
		break;
		case FMOD_ERR_DSP_NOTFOUND:
			os << "FMOD_ERR_DSP_NOTFOUND: DSP connection error. Couldn't find the DSP unit specified.";
		break;
		default:
			os << "Unknown error code";
		break;
		}
		debugLog(os.str());
		return true;
	}
	return false;
}

void SoundManager::setListenerPos(float x, float y)
{
	FMOD_VECTOR pos;
	pos.x = x;
	pos.y = y;
	pos.z = 0.0f;

	FMOD_VECTOR forward;
	forward.x = 0.0f;
	forward.y = 0.0f;
	forward.z = -1.0f;

	FMOD_VECTOR up;
	up.x = 0.0f;
	up.y = 1.0f;
	up.z = 0.0f;

	SoundCore::system->set3DListenerAttributes(0, &pos, NULL, &forward, &up);
}

void SoundManager::setSoundPos(void *channel, float x, float y)
{
	if (!channel)
		return;

	FMOD::Channel *pChannel = (FMOD::Channel*)channel;
	FMOD_VECTOR pos;
	pos.x = x;
	pos.y = y;
	pos.z = 0.0f;

	pChannel->set3DAttributes(&pos, NULL);
}

void SoundManager::setSoundRelative(void *channel, bool relative)
{
	if (!channel)
		return;

	FMOD::Channel *pChannel = (FMOD::Channel*)channel;

	FMOD_MODE mode = 0;
	pChannel->getMode(&mode);
	FMOD_MODE newmode = mode & ~(FMOD_3D_WORLDRELATIVE | FMOD_3D_HEADRELATIVE);
	if(relative)
		newmode |= (FMOD_3D_HEADRELATIVE | FMOD_3D);
	else
		newmode |= (FMOD_3D_WORLDRELATIVE | FMOD_3D);

	if (mode != newmode)
		pChannel->setMode(newmode);
}

static FMOD_RESULT s_soundHolderCallback(void *channel, FMOD_CHANNEL_CALLBACKTYPE type, void *commanddata1, void *commanddata2)
{
	if (!channel)
		return FMOD_ERR_INVALID_PARAM;

	SoundHolder *holder = NULL;
	FMOD::Channel *pChannel = (FMOD::Channel*)channel;
	pChannel->getUserData((void**)&holder);

	switch(type)
	{
		case FMOD_CHANNEL_CALLBACKTYPE_END:
			holder->unlinkSound(channel);
			break;
		default:
			return FMOD_ERR_INVALID_PARAM;
	}

	return FMOD_OK;
}


SoundHolder::~SoundHolder()
{
	unlinkAllSounds();
}

void SoundHolder::updateSoundPosition(float x, float y)
{
	if (activeSounds.size())
		for(std::vector<void*>::iterator it = activeSounds.begin(); it != activeSounds.end(); ++it)
			sound->setSoundPos(*it, x, y);
}

void SoundHolder::stopAllSounds()
{
	// activeSounds is modified by SoundManager::stopSfx(), which calls unkinkSound(), can't use iterator here
	while(activeSounds.size())
		sound->stopSfx(*activeSounds.begin());
}

void SoundHolder::unlinkSound(void *channel)
{
	FMOD::Channel *pChannel = (FMOD::Channel*)channel;
	pChannel->setUserData(NULL);
	pChannel->setCallback(NULL);
	std::vector<void*>::iterator where = std::remove(activeSounds.begin(), activeSounds.end(), channel);
	activeSounds.erase(where, activeSounds.end());
}

void SoundHolder::linkSound(void *channel)
{
	if (!channel)
		return;
	FMOD::Channel *pChannel = (FMOD::Channel*)channel;
	pChannel->setUserData(this);
	pChannel->setCallback(s_soundHolderCallback);
	activeSounds.push_back(channel);
}

void SoundHolder::unlinkAllSounds()
{
	while(activeSounds.size())
		unlinkSound(*activeSounds.begin());
}