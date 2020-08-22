/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2017 Soeren Apel <soeren@apelpie.net>
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
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef PULSEVIEW_PV_DATA_DECODESIGNAL_HPP
#define PULSEVIEW_PV_DATA_DECODESIGNAL_HPP

#include <atomic>
#include <deque>
#include <condition_variable>
#include <unordered_set>
#include <vector>

#include <QSettings>

#include <libsigrokdecode/libsigrokdecode.h>

#include <pv/data/decode/decoder.hpp>
#include <pv/data/decode/row.hpp>
#include <pv/data/decode/rowdata.hpp>
#include <pv/data/signalbase.hpp>
#include <pv/util.hpp>

using std::atomic;
using std::condition_variable;
using std::deque;
using std::map;
using std::mutex;
using std::vector;
using std::shared_ptr;

using pv::data::decode::Annotation;
using pv::data::decode::DecodeBinaryClassInfo;
using pv::data::decode::DecodeChannel;
using pv::data::decode::Decoder;
using pv::data::decode::Row;
using pv::data::decode::RowData;

namespace pv {
class Session;

namespace data {

class Logic;
class LogicSegment;
class SignalBase;
class SignalData;

struct DecodeBinaryDataChunk
{
	vector<uint8_t> data;
	uint64_t sample;   ///< Number of the sample where this data was provided by the PD
};

struct DecodeBinaryClass
{
	const Decoder* decoder;
	const DecodeBinaryClassInfo* info;
	deque<DecodeBinaryDataChunk> chunks;
};

struct DecodeSegment
{
	// Constructor is a no-op
	DecodeSegment() { };
	// Copy constructor is a no-op
	DecodeSegment(DecodeSegment&& ds) { (void)ds; };

	map<const Row*, RowData> annotation_rows;  // Note: Row is the same for all segments while RowData is not
	pv::util::Timestamp start_time;
	double samplerate;
	int64_t samples_decoded_incl, samples_decoded_excl;
	vector<DecodeBinaryClass> binary_classes;
	deque<const Annotation*> all_annotations;
};

class DecodeSignal : public SignalBase
{
	Q_OBJECT

private:
	static const double DecodeMargin;
	static const double DecodeThreshold;
	static const int64_t DecodeChunkLength;

public:
	DecodeSignal(pv::Session &session);
	virtual ~DecodeSignal();

	bool is_decode_signal() const;
	const vector< shared_ptr<Decoder> >& decoder_stack() const;

	void stack_decoder(const srd_decoder *decoder, bool restart_decode=true);
	void remove_decoder(int index);
	bool toggle_decoder_visibility(int index);

	void reset_decode(bool shutting_down = false);
	void begin_decode();
	void pause_decode();
	void resume_decode();
	bool is_paused() const;

	const vector<decode::DecodeChannel> get_channels() const;
	void auto_assign_signals(const shared_ptr<Decoder> dec);
	void assign_signal(const uint16_t channel_id, const SignalBase *signal);
	int get_assigned_signal_count() const;

	void set_initial_pin_state(const uint16_t channel_id, const int init_state);

	virtual double get_samplerate() const;
	const pv::util::Timestamp start_time() const;

	/**
	 * Returns the number of samples that can be worked on,
	 * i.e. the number of samples where samples are available
	 * for all connected channels.
	 */
	int64_t get_working_sample_count(uint32_t segment_id) const;

	/**
	 * Returns the number of processed samples. Newly generated annotations will
	 * have sample numbers greater than this.
	 *
	 * If include_processing is true, this number will include the ones being
	 * currently processed (in case the decoder stack is running). In this case,
	 * newly generated annotations will have sample numbers smaller than this.
	 */
	int64_t get_decoded_sample_count(uint32_t segment_id,
		bool include_processing) const;

	vector<Row*> get_rows(bool visible_only=false);
	vector<const Row*> get_rows(bool visible_only=false) const;

	uint64_t get_annotation_count(const Row* row, uint32_t segment_id) const;

	/**
	 * Extracts annotations from a single row into a vector.
	 * Note: The annotations may be unsorted and only annotations that fully
	 * fit into the sample range are considered.
	 */
	void get_annotation_subset(deque<const Annotation*> &dest, const Row* row,
		uint32_t segment_id, uint64_t start_sample, uint64_t end_sample) const;

	/**
	 * Extracts annotations from all rows into a vector.
	 * Note: The annotations may be unsorted and only annotations that fully
	 * fit into the sample range are considered.
	 */
	void get_annotation_subset(deque<const Annotation*> &dest, uint32_t segment_id,
		uint64_t start_sample, uint64_t end_sample) const;

	uint32_t get_binary_data_chunk_count(uint32_t segment_id,
		const Decoder* dec, uint32_t bin_class_id) const;
	void get_binary_data_chunk(uint32_t segment_id, const Decoder* dec,
		uint32_t bin_class_id, uint32_t chunk_id, const vector<uint8_t> **dest,
		uint64_t *size);
	void get_merged_binary_data_chunks_by_sample(uint32_t segment_id,
		const Decoder* dec, uint32_t bin_class_id,
		uint64_t start_sample, uint64_t end_sample,
		vector<uint8_t> *dest) const;
	void get_merged_binary_data_chunks_by_offset(uint32_t segment_id,
		const Decoder* dec, uint32_t bin_class_id,
		uint64_t start, uint64_t end,
		vector<uint8_t> *dest) const;
	const DecodeBinaryClass* get_binary_data_class(uint32_t segment_id,
		const Decoder* dec, uint32_t bin_class_id) const;

	const deque<const Annotation*>* get_all_annotations_by_segment(uint32_t segment_id) const;

	virtual void save_settings(QSettings &settings) const;

	virtual void restore_settings(QSettings &settings);

private:
	bool all_input_segments_complete(uint32_t segment_id) const;
	uint32_t get_input_segment_count() const;
	double get_input_samplerate(uint32_t segment_id) const;

	Decoder* get_decoder_by_instance(const srd_decoder *const srd_dec);

	void update_channel_list();

	void commit_decoder_channels();

	void mux_logic_samples(uint32_t segment_id, const int64_t start, const int64_t end);
	void logic_mux_proc();

	void decode_data(const int64_t abs_start_samplenum, const int64_t sample_count,
		const shared_ptr<LogicSegment> input_segment);
	void decode_proc();

	void start_srd_session();
	void terminate_srd_session();
	void stop_srd_session();

	void connect_input_notifiers();

	void create_decode_segment();

	static void annotation_callback(srd_proto_data *pdata, void *decode_signal);
	static void binary_callback(srd_proto_data *pdata, void *decode_signal);

Q_SIGNALS:
	void decoder_stacked(void* decoder); ///< decoder is of type decode::Decoder*
	void decoder_removed(void* decoder); ///< decoder is of type decode::Decoder*
	void new_annotations(); // TODO Supply segment for which they belong to
	void new_binary_data(unsigned int segment_id, void* decoder, unsigned int bin_class_id);
	void decode_reset();
	void decode_finished();
	void channels_updated();
	void annotation_visibility_changed();

private Q_SLOTS:
	void on_capture_state_changed(int state);
	void on_data_cleared();
	void on_data_received();

	void on_annotation_visibility_changed();

private:
	pv::Session &session_;

	vector<decode::DecodeChannel> channels_;

	struct srd_session *srd_session_;

	shared_ptr<Logic> logic_mux_data_;
	uint32_t logic_mux_unit_size_;
	bool logic_mux_data_invalid_;

	vector< shared_ptr<Decoder> > stack_;
	bool stack_config_changed_;

	vector<DecodeSegment> segments_;
	uint32_t current_segment_id_;

	mutable mutex input_mutex_, output_mutex_, decode_pause_mutex_, logic_mux_mutex_;
	mutable condition_variable decode_input_cond_, decode_pause_cond_,
		logic_mux_cond_;

	std::thread decode_thread_, logic_mux_thread_;
	atomic<bool> decode_interrupt_, logic_mux_interrupt_;

	bool decode_paused_;
};

} // namespace data
} // namespace pv

#endif // PULSEVIEW_PV_DATA_DECODESIGNAL_HPP
