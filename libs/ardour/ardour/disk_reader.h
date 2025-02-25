/*
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_disk_reader_h__
#define __ardour_disk_reader_h__

#include "pbd/i18n.h"

#include "ardour/disk_io.h"
#include "ardour/midi_buffer.h"

namespace ARDOUR
{

class Playlist;
class AudioPlaylist;
class MidiPlaylist;
template<typename T> class MidiRingBuffer;

class LIBARDOUR_API DiskReader : public DiskIOProcessor
{
public:
	DiskReader (Session&, std::string const & name, DiskIOProcessor::Flag f = DiskIOProcessor::Flag (0));
	~DiskReader ();

	bool set_name (std::string const & str);
	std::string display_name() const { return std::string (_("player")); }

	static samplecnt_t chunk_samples() { return _chunk_samples; }
	static samplecnt_t default_chunk_samples ();
	static void set_chunk_samples (samplecnt_t n) { _chunk_samples = n; }

	void run (BufferSet& /*bufs*/, samplepos_t /*start_sample*/, samplepos_t /*end_sample*/, double speed, pframes_t /*nframes*/, bool /*result_required*/);
	void realtime_handle_transport_stopped ();
	void realtime_locate ();
	bool overwrite_existing_buffers ();
	void set_pending_overwrite ();

	int set_state (const XMLNode&, int version);

	PBD::Signal0<void>            AlignmentStyleChanged;

	float buffer_load() const;

	void move_processor_automation (boost::weak_ptr<Processor>, std::list<Evoral::RangeMove<samplepos_t> > const &);

	/* called by the Butler in a non-realtime context */

	int do_refill () {
		return refill (_sum_buffer, _mixdown_buffer, _gain_buffer, 0);
	}

	/** For non-butler contexts (allocates temporary working buffers)
	 *
	 * This accessible method has a default argument; derived classes
	 * must inherit the virtual method that we call which does NOT
	 * have a default argument, to avoid complications with inheritance
	 */
	int do_refill_with_alloc (bool partial_fill = true) {
		return _do_refill_with_alloc (partial_fill);
	}

	bool pending_overwrite () const;

	// Working buffers for do_refill (butler thread)
	static void allocate_working_buffers();
	static void free_working_buffers();

	void adjust_buffering ();

	bool can_internal_playback_seek (sampleoffset_t distance);
	void internal_playback_seek (sampleoffset_t distance);
	int seek (samplepos_t sample, bool complete_refill = false);

	static PBD::Signal0<void> Underrun;

	void playlist_modified ();
	void reset_tracker ();

	bool declick_in_progress () const;

	static void set_midi_readahead_samples (samplecnt_t samples_ahead) { midi_readahead = samples_ahead; }

	/* inc/dec variants MUST be called as part of the process call tree, before any
	   disk readers are invoked. We use it when the session needs the
	   transport (and thus effective read position for DiskReaders) to keep
	   advancing as part of syncing up with a transport master, but we
	   don't want any actual disk output yet because we are still not
	   synced.
	*/

	static void inc_no_disk_output () { g_atomic_int_inc (&_no_disk_output); }
	static void dec_no_disk_output();
	static bool no_disk_output () { return g_atomic_int_get (&_no_disk_output); }

protected:
	friend class Track;
	friend class MidiTrack;

	struct ReaderChannelInfo : public DiskIOProcessor::ChannelInfo {
		ReaderChannelInfo (samplecnt_t buffer_size)
			: DiskIOProcessor::ChannelInfo (buffer_size)
		{
			resize (buffer_size);
		}
		void resize (samplecnt_t);
	};

	XMLNode& state ();

	void resolve_tracker (Evoral::EventSink<samplepos_t>& buffer, samplepos_t time);

	void playlist_changed (const PBD::PropertyChange&);
	int use_playlist (DataType, boost::shared_ptr<Playlist>);
	void playlist_ranges_moved (std::list< Evoral::RangeMove<samplepos_t> > const &, bool);

	int add_channel_to (boost::shared_ptr<ChannelList>, uint32_t how_many);

	class DeclickAmp
	{
		public:
			DeclickAmp (samplecnt_t sample_rate);

			void apply_gain (AudioBuffer& buf, samplecnt_t n_samples, const float target);

			float gain () const { return _g; }
			void set_gain (float g) { _g = g; }

		private:
			float _a;
			float _l;
			float _g;
	};

private:
	/** The number of samples by which this diskstream's output should be delayed
	    with respect to the transport sample.  This is used for latency compensation.
	*/
	samplepos_t   overwrite_sample;
	mutable gint  _pending_overwrite;
	bool          overwrite_queued;
	IOChange      input_change_pending;
	samplepos_t   file_sample[DataType::num_types];

	DeclickAmp     _declick_amp;
	sampleoffset_t _declick_offs;

	int _do_refill_with_alloc (bool partial_fill);

	static samplecnt_t _chunk_samples;
	static samplecnt_t midi_readahead;
	static gint       _no_disk_output;

	int audio_read (PBD::PlaybackBuffer<Sample>*,
	                Sample* sum_buffer,
	                Sample* mixdown_buffer,
	                float*  gain_buffer,
	                samplepos_t& start, samplecnt_t cnt,
	                int channel, bool reversed);
	int midi_read (samplepos_t& start, samplecnt_t cnt, bool reversed);

	static Sample* _sum_buffer;
	static Sample* _mixdown_buffer;
	static gain_t* _gain_buffer;

	int refill (Sample* sum_buffer, Sample* mixdown_buffer, float* gain_buffer, samplecnt_t fill_level);
	int refill_audio (Sample* sum_buffer, Sample *mixdown_buffer, float *gain_buffer, samplecnt_t fill_level);
	int refill_midi ();

	sampleoffset_t calculate_playback_distance (pframes_t);

	void get_midi_playback (MidiBuffer& dst, samplepos_t start_sample, samplepos_t end_sample, MonitorState, BufferSet&, double speed, samplecnt_t distance);
};

} // namespace

#endif /* __ardour_disk_reader_h__ */
