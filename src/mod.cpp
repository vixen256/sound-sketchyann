#include "diva.h"
#include "safetyhook.hpp"
#include <chrono>
#include <optional>
#include <toml++/impl/forward_declarations.hpp>

using namespace diva;

bool
insert_new_sound (const char *filepath, const char *name, f32 volume) {
	i32 res = 0;

	string inout = string (filepath);
	if (!ResolveFilePath (&inout)) {
		printf ("[Sound] Could not find file %s\n", filepath);
		return false;
	}

	auto input_ctx = avformat_alloc_context ();
	res            = avformat_open_input (&input_ctx, inout.c_str (), nullptr, nullptr);
	if (res < 0 || input_ctx == nullptr) {
		printf ("[Sound] Could not open %s\n", inout.c_str ());
		return false;
	}

	res = avformat_find_stream_info (input_ctx, nullptr);
	if (res < 0) {
		avformat_free_context (input_ctx);
		printf ("[Sound] Could not find stream info for %s: %s\n", inout.c_str (), av_err2str (res));
		return false;
	}

	const AVCodec *decoder = nullptr;
	auto stream_index      = av_find_best_stream (input_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, &decoder, 0);
	if (stream_index < 0 || decoder == nullptr) {
		avformat_free_context (input_ctx);
		printf ("[Sound] Could not find audio stream for %s: %s\n", inout.c_str (), av_err2str (stream_index));
		return false;
	}

	auto stream      = input_ctx->streams[stream_index];
	auto decoder_ctx = avcodec_alloc_context3 (decoder);
	if (stream == nullptr || decoder_ctx == nullptr) {
		avformat_free_context (input_ctx);
		avcodec_free_context (&decoder_ctx);
		printf ("[Sound] Could not allocate decoding context for %s\n", inout.c_str ());
		return false;
	}

	res = avcodec_parameters_to_context (decoder_ctx, stream->codecpar);
	if (res < 0) {
		avformat_free_context (input_ctx);
		avcodec_free_context (&decoder_ctx);
		printf ("[Sound] Could not allocate decoding context for %s: %s\n", inout.c_str (), av_err2str (res));
		return false;
	}

	res = avcodec_open2 (decoder_ctx, decoder, nullptr);
	if (res < 0) {
		avformat_free_context (input_ctx);
		avcodec_free_context (&decoder_ctx);
		printf ("[Sound] Could not open decoder for %s: %s\n", inout.c_str (), av_err2str (res));
		return false;
	}

	res = av_seek_frame (input_ctx, stream_index, 0, 0);
	if (res < 0) {
		avformat_free_context (input_ctx);
		avcodec_free_context (&decoder_ctx);
		printf ("[Sound] Could not seek to 0 for %s: %s\n", inout.c_str (), av_err2str (res));
		return false;
	}

	auto resample_ctx = swr_alloc ();
	if (resample_ctx == nullptr) {
		avformat_free_context (input_ctx);
		avcodec_free_context (&decoder_ctx);
		printf ("[Sound] Could not allocate resample context\n");
		return false;
	}

	av_opt_set_chlayout (resample_ctx, "in_chlayout", &decoder_ctx->ch_layout, 0);
	av_opt_set_int (resample_ctx, "in_sample_rate", decoder_ctx->sample_rate, 0);
	av_opt_set_sample_fmt (resample_ctx, "in_sample_fmt", decoder_ctx->sample_fmt, 0);

	const AVChannelLayout channel_layout = AV_CHANNEL_LAYOUT_STEREO;
	av_opt_set_chlayout (resample_ctx, "out_chlayout", &channel_layout, 0);
	av_opt_set_int (resample_ctx, "out_sample_rate", 44100, 0);
	av_opt_set_sample_fmt (resample_ctx, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

	res = swr_init (resample_ctx);
	if (res < 0) {
		avformat_free_context (input_ctx);
		avcodec_free_context (&decoder_ctx);
		swr_free (&resample_ctx);
		printf ("[Sound] Could not init resample context: %s\n", av_err2str (res));
		return false;
	}

	auto encoder_codec = avcodec_find_encoder (AV_CODEC_ID_ADPCM_IMA_SSI);
	if (encoder_codec == nullptr) {
		avformat_free_context (input_ctx);
		avcodec_free_context (&decoder_ctx);
		swr_free (&resample_ctx);
		printf ("[Sound] Could not allocate encoder\n");
		return false;
	}

	auto encoder_ctx = avcodec_alloc_context3 (encoder_codec);
	if (encoder_ctx == nullptr) {
		avformat_free_context (input_ctx);
		avcodec_free_context (&decoder_ctx);
		swr_free (&resample_ctx);
		printf ("[Sound] Could not allocate encoder context\n");
		return false;
	}

	encoder_ctx->sample_rate = 44100;
	encoder_ctx->sample_fmt  = AV_SAMPLE_FMT_S16;
	encoder_ctx->ch_layout   = AV_CHANNEL_LAYOUT_STEREO;

	res = avcodec_open2 (encoder_ctx, encoder_codec, nullptr);
	if (res < 0) {
		avformat_free_context (input_ctx);
		avcodec_free_context (&decoder_ctx);
		avcodec_free_context (&encoder_ctx);
		swr_free (&resample_ctx);
		printf ("[Sound] Could not open encoder context: %s\n", av_err2str (res));
		return false;
	}

	u32 sample_count    = 0;
	bool getting_frames = true;
	std::vector<u8> encoded_samples;

	i32 encoder_frame_samples  = 0;
	auto encoder_frame         = av_frame_alloc ();
	encoder_frame->ch_layout   = AV_CHANNEL_LAYOUT_STEREO;
	encoder_frame->sample_rate = 44100;
	encoder_frame->format      = AV_SAMPLE_FMT_S16;
	encoder_frame->nb_samples  = encoder_ctx->frame_size;
	av_frame_get_buffer (encoder_frame, 0);

	while (getting_frames) {
		AVPacket *packet = av_packet_alloc ();
		res              = av_read_frame (input_ctx, packet);
		if (res < 0) {
			// Flush the decoder rather than instantly returning
			av_packet_free (&packet);
			packet         = nullptr;
			getting_frames = false;
		} else if (packet->stream_index != stream_index) {
			av_packet_free (&packet);
			continue;
		}

		res = avcodec_send_packet (decoder_ctx, packet);
		if (res < 0) {
			av_packet_free (&packet);
			break;
		}

		auto frame = av_frame_alloc ();
		res        = avcodec_receive_frame (decoder_ctx, frame);
		while (res >= 0) {
			auto resampled         = av_frame_alloc ();
			resampled->ch_layout   = AV_CHANNEL_LAYOUT_STEREO;
			resampled->sample_rate = 44100;
			resampled->format      = AV_SAMPLE_FMT_S16;

			if (frame->ch_layout.order == AV_CHANNEL_ORDER_UNSPEC) av_channel_layout_default (&frame->ch_layout, frame->ch_layout.nb_channels);
			res = swr_convert_frame (resample_ctx, resampled, frame);

			if (res < 0) {
				printf ("[Sound] Could not convert audio frame: %s\n", av_err2str (res));
				av_frame_free (&resampled);
				break;
			}

			sample_count += resampled->nb_samples;

			i32 read_samples      = std::min (resampled->nb_samples, encoder_ctx->frame_size - encoder_frame_samples);
			i32 remaining_samples = resampled->nb_samples - read_samples;
			memcpy (encoder_frame->data[0] + encoder_frame_samples * 4, resampled->data[0], read_samples * 4);
			encoder_frame_samples += read_samples;
			i32 resampled_offset = read_samples;
			while (encoder_frame_samples == encoder_ctx->frame_size) {
				encoder_frame_samples = 0;
				res                   = avcodec_send_frame (encoder_ctx, encoder_frame);
				if (res < 0) {
					printf ("[Sound] Could not encode audio frame: %s\n", av_err2str (res));
					break;
				}

				AVPacket *encoded_packet = av_packet_alloc ();
				res                      = avcodec_receive_packet (encoder_ctx, encoded_packet);
				while (res >= 0) {
					if (!(packet->flags & AV_PKT_FLAG_DISCARD))
						for (int i = 0; i < encoded_packet->size; i++)
							encoded_samples.push_back (encoded_packet->data[i]);
					res = avcodec_receive_packet (encoder_ctx, encoded_packet);
				}
				av_packet_free (&encoded_packet);

				if (remaining_samples > 0) {
					read_samples = std::min (remaining_samples, encoder_ctx->frame_size);
					remaining_samples -= read_samples;
					memcpy (encoder_frame->data[0] + encoder_frame_samples * 4, resampled->data[0] + resampled_offset * 4, read_samples * 4);
					encoder_frame_samples += read_samples;
					resampled_offset += read_samples;
				}
			}

			av_frame_free (&resampled);

			res = avcodec_receive_frame (decoder_ctx, frame);
		}

		av_frame_free (&frame);
		av_packet_free (&packet);
	}

	if (swr_get_delay (resample_ctx, decoder_ctx->sample_rate)) {
		auto resampled         = av_frame_alloc ();
		resampled->ch_layout   = AV_CHANNEL_LAYOUT_STEREO;
		resampled->sample_rate = 44100;
		resampled->format      = AV_SAMPLE_FMT_S16;

		res = swr_convert_frame (resample_ctx, resampled, nullptr);
		if (res < 0) {
			printf ("[Sound] Could not convert audio frame %s\n", av_err2str (res));
			avformat_free_context (input_ctx);
			avcodec_free_context (&decoder_ctx);
			avcodec_free_context (&encoder_ctx);
			swr_free (&resample_ctx);
			av_frame_free (&resampled);
			return false;
		}

		sample_count += resampled->nb_samples;

		i32 read_samples      = std::min (resampled->nb_samples, encoder_ctx->frame_size - encoder_frame_samples);
		i32 remaining_samples = resampled->nb_samples - read_samples;
		memcpy (encoder_frame->data[0] + encoder_frame_samples * 4, resampled->data[0], read_samples * 4);
		encoder_frame_samples += read_samples;
		i32 resampled_offset = read_samples;
		while (encoder_frame_samples == encoder_ctx->frame_size) {
			res = avcodec_send_frame (encoder_ctx, encoder_frame);
			if (res < 0) {
				encoder_frame_samples = 0;
				printf ("[Sound] Could not encode audio frame: %s\n", av_err2str (res));
				break;
			}

			AVPacket *encoded_packet = av_packet_alloc ();
			res                      = avcodec_receive_packet (encoder_ctx, encoded_packet);
			while (res >= 0) {
				for (int i = 0; i < encoded_packet->size; i++)
					encoded_samples.push_back (encoded_packet->data[i]);
				res = avcodec_receive_packet (encoder_ctx, encoded_packet);
			}
			av_packet_free (&encoded_packet);

			read_samples = std::min (remaining_samples, encoder_ctx->frame_size);
			remaining_samples -= read_samples;
			memcpy (encoder_frame->data[0] + encoder_frame_samples * 4, resampled->data[0] + resampled_offset * 4, read_samples * 4);
			encoder_frame_samples = read_samples;
			resampled_offset += read_samples;
		}

		av_frame_free (&resampled);
	}

	encoder_frame->nb_samples = encoder_frame_samples;
	avcodec_send_frame (encoder_ctx, encoder_frame);
	avcodec_send_frame (encoder_ctx, nullptr);
	encoder_frame->nb_samples = encoder_ctx->frame_size;
	av_frame_free (&encoder_frame);

	AVPacket *encoded_packet = av_packet_alloc ();
	res                      = avcodec_receive_packet (encoder_ctx, encoded_packet);
	while (res >= 0) {
		for (int i = 0; i < encoded_packet->size; i++)
			encoded_samples.push_back (encoded_packet->data[i]);
		res = avcodec_receive_packet (encoder_ctx, encoded_packet);
	}
	av_packet_free (&encoded_packet);

	auto diva_data = (u32 *)malloc (0x40 + encoded_samples.size ());
	memset (diva_data, 0, 0x40 + encoded_samples.size ());
	diva_data[0] = 'AVID';
	diva_data[2] = encoded_samples.size ();
	diva_data[3] = 44100;
	diva_data[4] = sample_count;
	diva_data[7] = 2;
	memcpy ((void *)((u64)diva_data + 0x40), encoded_samples.data (), encoded_samples.size ());

	avformat_free_context (input_ctx);
	avcodec_free_context (&decoder_ctx);
	avcodec_free_context (&encoder_ctx);
	swr_free (&resample_ctx);

	farc_file out_file;
	strcpy (out_file.name, name);
	out_file.offset          = 0;
	out_file.size            = 0x40 + encoded_samples.size ();
	out_file.size_compressed = 0x40 + encoded_samples.size ();
	out_file.flags           = FARC_NONE;

	sound_db_farc *out = allocate<sound_db_farc> (1);
	memset ((void *)out, 0, sizeof (sound_db_farc));
	out->file_path.capacity = 0x0F;
	out->file_path.assign (filepath);
	out->ready     = true;
	out->farc.data = diva_data;
	out->farc.files.push_back (out_file);
	out->farc.signature = FARC_FArc;

	sound_db_property prop;
	prop.farc            = out;
	prop.farc_file_index = 0;
	prop.volume          = volume;
	prop.loop_start      = 0;
	prop.loop_end        = 0;
	prop.release_time    = 0.5;

	auto elem               = allocate<mapElement<string, sound_db_property>> (1);
	elem->left              = sound_work->root;
	elem->parent            = sound_work->root;
	elem->right             = sound_work->root;
	elem->isBlack           = false;
	elem->isNull            = false;
	elem->pair.key.length   = 0;
	elem->pair.key.capacity = 0x0F;
	elem->pair.key.assign (name);
	elem->pair.value = prop;

	FUNCTION_PTR (void, SoundDbPropertyInsertNode, 0x140157A50, map<string, sound_db_property> *root, pair<mapElement<string, sound_db_property> *, u32> *location,
	              mapElement<string, sound_db_property> *element);
	mapElement<string, sound_db_property> *parent = sound_work->root->parent;

	auto loc      = pair<mapElement<string, sound_db_property> *, u32> (sound_work->root->parent, 0);
	auto try_node = sound_work->root->parent;
	while (!try_node->isNull) {
		loc.key = try_node;
		if (*try_node->key () < elem->pair.key) {
			loc.value = 0;
			try_node  = try_node->right;
		} else {
			loc.value = 1;
			try_node  = try_node->left;
		}
	}

	SoundDbPropertyInsertNode (sound_work, &loc, elem);

	return true;
}

std::random_device rd;
std::mt19937 gen (rd ());
std::discrete_distribution<u64> logo_sega_sounds_generator;
std::discrete_distribution<u64> logo_crypton_sounds_generator;
std::discrete_distribution<u64> logo_piapro_sounds_generator;
std::discrete_distribution<u64> logo_megastar_sounds_generator;
std::discrete_distribution<u64> logo_title_sounds_generator;
std::discrete_distribution<u64> pvsel_enter_sounds_generator;
std::discrete_distribution<u64> pvsel_exit_sounds_generator;
std::discrete_distribution<u64> result_fail_sounds_generator;
std::discrete_distribution<u64> result_standard_sounds_generator;
std::discrete_distribution<u64> result_great_sounds_generator;
std::discrete_distribution<u64> result_excellent_sounds_generator;
std::discrete_distribution<u64> result_perfect_sounds_generator;
std::discrete_distribution<u64> pvgame_enter_sounds_generator;
std::discrete_distribution<u64> pvgame_low_health_sounds_generator;
std::discrete_distribution<u64> pvgame_fifty_combo_sounds_generator;

struct Sound {
	std::string filepath;
	std::string name;
	f64 weight;
	f32 volume;
	std::optional<std::set<i32>> charas;
	std::optional<std::set<i32>> module_ids;
};

std::vector<Sound> logo_sega_sounds;
std::vector<Sound> logo_crypton_sounds;
std::vector<Sound> logo_piapro_sounds;
std::vector<Sound> logo_megastar_sounds;
std::vector<Sound> logo_title_sounds;
std::vector<Sound> pvsel_enter_sounds;
std::vector<Sound> pvsel_exit_sounds;
std::vector<Sound> result_fail_sounds;
std::vector<Sound> result_standard_sounds;
std::vector<Sound> result_great_sounds;
std::vector<Sound> result_excellent_sounds;
std::vector<Sound> result_perfect_sounds;
std::vector<Sound> pvgame_enter_sounds;
std::vector<Sound> pvgame_low_health_sounds;
std::vector<Sound> pvgame_fifty_combo_sounds;

void
generate_weights (const std::vector<Sound> &sounds, std::discrete_distribution<u64> &generator, const std::string &except) {
	bool contains_except = false;
	u64 except_index     = 0;
	for (u64 i = 0; i < sounds.size (); i++) {
		if (sounds[i].name == except) {
			contains_except = true;
			except_index    = i;
			break;
		}
	}
	if (generator.probabilities ().size () >= sounds.size () && !contains_except) return;

	bool skip_except = false;
	if (contains_except) {
		if (sounds[except_index].module_ids.has_value ()) {
			u64 i;
			for (i = 0; i < sounds.size (); i++) {
				if (i == skip_except) continue;
				if (!sounds[i].module_ids.has_value ()) continue;
				for (auto m : *sounds[except_index].module_ids)
					if (sounds[i].module_ids->contains (m)) break;
			}
			if (i == sounds.size ()) skip_except = true;
		} else if (sounds[except_index].charas.has_value ()) {
			u64 i;
			for (i = 0; i < sounds.size (); i++) {
				if (i == skip_except) continue;
				if (!sounds[i].charas.has_value ()) continue;
				for (auto c : *sounds[except_index].charas)
					if (sounds[i].charas->contains (c)) break;
			}
			if (i == sounds.size ()) skip_except = true;
		} else {
			u64 i;
			for (i = 0; i < sounds.size (); i++) {
				if (i == skip_except) continue;
				if (!sounds[i].charas.has_value () && !sounds[i].module_ids.has_value ()) break;
			}
			if (i == sounds.size ()) skip_except = true;
		}
	}

	if (!skip_except) skip_except = sounds.size () == 1;

	std::vector<f64> weights (sounds.size ());

	for (auto i = 0; i != sounds.size (); i++)
		if (!skip_except && sounds[i].name == except) weights[i] = 0.0;
		else weights[i] = sounds[i].weight;

	generator.param (std::discrete_distribution<u64>::param_type (weights.begin (), weights.end ()));
}

void
generate_all_weights (const std::string &except) {
	generate_weights (logo_sega_sounds, logo_sega_sounds_generator, except);
	generate_weights (logo_crypton_sounds, logo_crypton_sounds_generator, except);
	generate_weights (logo_piapro_sounds, logo_piapro_sounds_generator, except);
	generate_weights (logo_megastar_sounds, logo_megastar_sounds_generator, except);
	generate_weights (logo_title_sounds, logo_title_sounds_generator, except);
	generate_weights (pvsel_enter_sounds, pvsel_enter_sounds_generator, except);
	generate_weights (pvsel_exit_sounds, pvsel_exit_sounds_generator, except);
	generate_weights (result_fail_sounds, result_fail_sounds_generator, except);
	generate_weights (result_standard_sounds, result_standard_sounds_generator, except);
	generate_weights (result_great_sounds, result_great_sounds_generator, except);
	generate_weights (result_excellent_sounds, result_excellent_sounds_generator, except);
	generate_weights (result_perfect_sounds, result_perfect_sounds_generator, except);
	generate_weights (pvgame_enter_sounds, pvgame_enter_sounds_generator, except);
	generate_weights (pvgame_low_health_sounds, pvgame_low_health_sounds_generator, except);
	generate_weights (pvgame_fifty_combo_sounds, pvgame_fifty_combo_sounds_generator, except);
}

enum class LocationFilter {
	LogoSega,
	LogoCrypton,
	LogoPiapro,
	LogoMegastar,
	LogoTitle,
	PVSelEnter,
	PVSelExit,
	ResultFail,
	ResultAnyClear,
	ResultStandard,
	ResultGreat,
	ResultExcellent,
	ResultPerfect,
	PVGameEnter,
	PVGameLowHealth,
	PVGameFiftyCombo,
};

std::optional<LocationFilter>
get_location_filter (const std::string &location_original, const std::string &filter_original) {
	std::string location (location_original);
	std::string filter (filter_original);

	for (auto i = 0; i != location.length (); i++)
		location[i] = std::tolower (location[i]);
	for (auto i = 0; i != filter.length (); i++)
		filter[i] = std::tolower (filter[i]);

	if (location == "logo" && filter == "sega") return LocationFilter::LogoSega;
	else if (location == "logo" && filter == "crypton") return LocationFilter::LogoCrypton;
	else if (location == "logo" && filter == "piapro") return LocationFilter::LogoPiapro;
	else if (location == "logo" && filter == "megastar") return LocationFilter::LogoMegastar;
	else if (location == "logo" && filter == "title") return LocationFilter::LogoTitle;
	else if (location == "pvsel" && filter == "enter") return LocationFilter::PVSelEnter;
	else if (location == "pvsel" && filter == "exit") return LocationFilter::PVSelExit;
	else if (location == "result" && filter == "fail") return LocationFilter::ResultFail;
	else if (location == "result" && filter == "AnyClear") return LocationFilter::ResultAnyClear;
	else if (location == "result" && filter == "standard") return LocationFilter::ResultStandard;
	else if (location == "result" && filter == "great") return LocationFilter::ResultGreat;
	else if (location == "result" && filter == "excellent") return LocationFilter::ResultExcellent;
	else if (location == "result" && filter == "perfect") return LocationFilter::ResultPerfect;
	else if (location == "pvgame" && filter == "enter") return LocationFilter::PVGameEnter;
	else if (location == "pvgame" && filter == "lowhealth") return LocationFilter::PVGameLowHealth;
	else if (location == "pvgame" && filter == "fiftycombo") return LocationFilter::PVGameFiftyCombo;
	else return std::nullopt;
}

std::map<std::string, i32> chara_names = {{"MIK", 0},  {"MIKU", 0}, {"RIN", 1},   {"LEN", 2}, {"LUK", 3},   {"LUKA", 3}, {"NER", 4},    {"NERU", 4}, {"HAK", 5},
                                          {"HAKU", 5}, {"KAI", 6},  {"KAITO", 6}, {"MEI", 7}, {"MEIKO", 7}, {"SAK", 8},  {"SAKINE", 8}, {"TET", 9},  {"TETO", 9}};
void
parse_sound_dbs () {
	for (auto it = romDirs->begin (); it != romDirs->end (); it++) {
		if (strcmp (it->c_str (), "./") == 0) continue;

		char buf[MAX_PATH];
		sprintf (buf, "%s/rom/sound/sound_db.toml", it->c_str ());
		auto str = string (buf);
		if (!ResolveFilePath (&str)) continue;

		auto db = toml::parse_file (str.c_str ());
		if (db.failed ()) continue;
		auto sounds = db["sound"].as_array ();
		if (!sounds) continue;

		for (auto j = sounds->begin (); j != sounds->end (); j++) {
			auto tbl = j->as_table ();
			if (!tbl) continue;

			auto file       = tbl->get ("file");
			auto location   = tbl->get ("location");
			auto filter     = tbl->get ("filter");
			auto weight     = tbl->get ("weight");
			auto volume     = tbl->get ("volume");
			auto charas     = tbl->get ("charas");
			auto module_ids = tbl->get ("module_ids");

			if (!file || !location || !filter || !file->is_string () || !location->is_string () || !filter->is_string ()) continue;

			Sound entry;
			entry.filepath.assign (file->as_string ()->get ());
			auto start = entry.filepath.rfind ('/');
			auto end   = entry.filepath.rfind ('.');
			entry.name.assign (entry.filepath.substr (start ? start + 1 : 0, end ? end - start - 1 : entry.filepath.length () - start - 1));
			entry.weight     = weight ? weight->value_or (1.0) : 1.0;
			entry.volume     = volume ? volume->value_or (1.0) : 1.0;
			entry.charas     = std::nullopt;
			entry.module_ids = std::nullopt;

			if (charas && charas->is_array () && !charas->as_array ()->empty () && charas->is_homogeneous (toml::node_type::string)) {
				entry.charas = std::optional (std::set<i32> ());
				for (auto &c : *charas->as_array ())
					if (chara_names.contains (c.as_string ()->get ())) entry.charas->insert (chara_names[c.as_string ()->get ()]);
					else printf ("[Sound] Unknown chara %s\n", c.as_string ()->get ().c_str ());
			}

			if (module_ids && module_ids->is_array () && !module_ids->as_array ()->empty () && module_ids->is_homogeneous (toml::node_type::integer)) {
				entry.module_ids = std::optional (std::set<i32> ());
				for (auto &m : *module_ids->as_array ())
					entry.module_ids->insert (m.as_integer ()->get ());
			}

			auto loc_filter = get_location_filter (location->as_string ()->get (), filter->as_string ()->get ());
			if (!loc_filter.has_value ()) {
				printf ("[Sound] Unknown location/filter combo %s %s\n", location->as_string ()->get ().c_str (), location->as_string ()->get ().c_str ());
				continue;
			}

			if (!insert_new_sound (entry.filepath.c_str (), entry.name.c_str (), entry.volume)) continue;
			switch (loc_filter.value ()) {
			case LocationFilter::LogoSega: logo_sega_sounds.push_back (entry); break;
			case LocationFilter::LogoCrypton: logo_crypton_sounds.push_back (entry); break;
			case LocationFilter::LogoPiapro: logo_piapro_sounds.push_back (entry); break;
			case LocationFilter::LogoMegastar: logo_megastar_sounds.push_back (entry); break;
			case LocationFilter::LogoTitle: logo_title_sounds.push_back (entry); break;
			case LocationFilter::PVSelEnter: pvsel_enter_sounds.push_back (entry); break;
			case LocationFilter::PVSelExit: pvsel_exit_sounds.push_back (entry); break;
			case LocationFilter::ResultFail: result_fail_sounds.push_back (entry); break;
			case LocationFilter::ResultAnyClear:
				result_standard_sounds.push_back (entry);
				result_great_sounds.push_back (entry);
				result_excellent_sounds.push_back (entry);
				result_perfect_sounds.push_back (entry);
				break;
			case LocationFilter::ResultStandard: result_standard_sounds.push_back (entry); break;
			case LocationFilter::ResultGreat: result_great_sounds.push_back (entry); break;
			case LocationFilter::ResultExcellent: result_excellent_sounds.push_back (entry); break;
			case LocationFilter::ResultPerfect: result_perfect_sounds.push_back (entry); break;
			case LocationFilter::PVGameEnter: pvgame_enter_sounds.push_back (entry); break;
			case LocationFilter::PVGameLowHealth: pvgame_low_health_sounds.push_back (entry); break;
			case LocationFilter::PVGameFiftyCombo: pvgame_fifty_combo_sounds.push_back (entry); break;
			};
		}
	}

	generate_all_weights ("");
}

bool quieten    = false;
bool playing_se = false;
std::string playing_se_name;
f32 new_volumes[3];
SafetyHookMid sound_stream_array_ctrl_hook;
void
sound_stream_array_ctrl (SafetyHookContext &ctx) {
	auto sound_stream = ctx.rcx;
	auto index        = ctx.rdi - 1;
	if (quieten) {
		quieten                       = false;
		new_volumes[index]            = *(f32 *)(sound_stream + 0x28) * 0.5;
		*(f32 *)(sound_stream + 0x28) = new_volumes[index];
		*(i32 *)(sound_stream + 0x40) = 0;
	}

	if (!playing_se) return;

	if (*(f32 *)(sound_stream + 0x28) != new_volumes[index]) {
		new_volumes[index]            = *(f32 *)(sound_stream + 0x28) * 0.5;
		*(f32 *)(sound_stream + 0x28) = new_volumes[index];
	}

	if (!SoundEffectCanPlay (1, playing_se_name.c_str ())) {
		playing_se                    = false;
		*(f32 *)(sound_stream + 0x28) = new_volumes[index] * 2.0;
	}
}

void
play_se (const std::vector<Sound> &sounds, std::discrete_distribution<u64> &generator, bool should_quieten = true, std::optional<i32> chara = std::nullopt,
         std::optional<i32> module_id = std::nullopt) {
	SoundEffectRelease (1, playing_se_name.c_str (), true);
	if (sounds.empty ()) return;

	if (chara || module_id) {
		bool has_module         = false;
		bool has_current_module = false;
		bool has_chara          = false;
		bool has_current_chara  = false;
		for (auto i = 0; i != sounds.size (); i++) {
			if (generator.probabilities ()[i] > 0.0 && sounds[i].module_ids.has_value ()) {
				has_module = true;
				if (sounds[i].module_ids->contains (*module_id)) has_current_module = true;
			}
			if (generator.probabilities ()[i] > 0.0 && sounds[i].charas.has_value ()) {
				has_chara = true;
				if (sounds[i].charas->contains (*chara)) has_current_chara = true;
			}
		}

		std::vector<f64> weights (sounds.size ());
		bool set_weights = false;
		if (has_module && has_current_module) {
			for (auto i = 0; i != sounds.size (); i++)
				if (generator.probabilities ()[i] > 0.0 && sounds[i].module_ids.has_value () && sounds[i].module_ids->contains (*module_id)) weights[i] = sounds[i].weight;
				else weights[i] = 0.0;

			for (auto w : weights)
				if (w > 0.0) set_weights = true;
			if (set_weights) generator.param (std::discrete_distribution<u64>::param_type (weights.begin (), weights.end ()));
		}

		if (!set_weights && has_chara && has_current_chara) {
			for (auto i = 0; i != sounds.size (); i++)
				if (generator.probabilities ()[i] > 0.0 && sounds[i].charas.has_value () && sounds[i].charas->contains (*chara)) weights[i] = sounds[i].weight;
				else weights[i] = 0.0;

			for (auto w : weights)
				if (w > 0.0) set_weights = true;
			if (set_weights) generator.param (std::discrete_distribution<u64>::param_type (weights.begin (), weights.end ()));
		}

		if (!set_weights) {
			for (auto i = 0; i != sounds.size (); i++)
				if (sounds[i].module_ids.has_value () || sounds[i].charas.has_value ()) weights[i] = 0.0;
				else weights[i] = sounds[i].weight;

			for (auto w : weights)
				if (w > 0.0) set_weights = true;
			if (set_weights) generator.param (std::discrete_distribution<u64>::param_type (weights.begin (), weights.end ()));
			else return;
		}
	}

	auto sound = sounds[generator (gen)];
	PlaySoundEffect (sound.name.c_str (), 1.0);
	generate_all_weights (sound.name);

	quieten    = should_quieten;
	playing_se = true;
	playing_se_name.assign (sound.name);
}

SafetyHookMid data_init_hook;
void
data_init (SafetyHookContext &ctx) {
	std::thread t (parse_sound_dbs);
	t.detach ();
}

SafetyHookMid pvgame_set_rank_hook;
void
pvgame_set_rank (SafetyHookContext &ctx) {
	auto rank = *(i32 *)ctx.rsi + 1;

	auto sounds    = result_fail_sounds;
	auto generator = result_fail_sounds_generator;
	switch (rank) {
	case 3:
		sounds    = result_standard_sounds;
		generator = result_standard_sounds_generator;
		break;
	case 4:
		sounds    = result_great_sounds;
		generator = result_great_sounds_generator;
		break;
	case 5:
		sounds    = result_excellent_sounds;
		generator = result_excellent_sounds_generator;
		break;
	case 6:
		sounds    = result_perfect_sounds;
		generator = result_perfect_sounds_generator;
		break;
	}

	auto load_data           = (u64)GetPvLoadData ();
	auto module              = *(i32 *)(load_data + 0xA8);
	std::optional<i32> chara = std::nullopt;
	for (auto it = modules->begin (); it != modules->end (); it++) {
		if (it->id == module) {
			chara = std::optional (it->chara);
			break;
		}
	}

	play_se (sounds, generator, true, chara, module);
}

SafetyHookMid nswgam_pvsel_enter_hook;
void
nswgam_pvsel_enter (SafetyHookContext &ctx) {
	play_se (pvsel_enter_sounds, pvsel_enter_sounds_generator);
}

SafetyHookMid nswgam_pvsel_exit_hook;
void
nswgam_pvsel_exit (SafetyHookContext &ctx) {
	if (pvsel_exit_sounds.empty ()) return;

	play_se (pvsel_exit_sounds, pvsel_exit_sounds_generator);
	// Skip se_ft_cansel
	ctx.rax = 3;
	ctx.rip = 0x1406F09B6;
}

SafetyHookMid nswgam_pvgame_enter_hook;
void
nswgam_pvgame_enter (SafetyHookContext &ctx) {
	if (pvgame_enter_sounds.empty ()) return;

	auto load_data           = (u64)GetPvLoadData ();
	auto module              = *(i32 *)(load_data + 0xA8);
	std::optional<i32> chara = std::nullopt;
	for (auto it = modules->begin (); it != modules->end (); it++) {
		if (it->id == module) {
			chara = std::optional (it->chara);
			break;
		}
	}

	play_se (pvgame_enter_sounds, pvgame_enter_sounds_generator, true, chara, module);
	ctx.rip = 0x1406F8C7E; // Skip se_ft_music_selector_enter_01
}

SafetyHookMid cs_pvsel_enter_hook;
void
cs_pvsel_enter (SafetyHookContext &ctx) {
	play_se (pvsel_enter_sounds, pvsel_enter_sounds_generator);
}

SafetyHookMid cs_pvsel_exit_hook;
void
cs_pvsel_exit (SafetyHookContext &ctx) {
	if (IsSurvival () || pvsel_exit_sounds.empty ()) return;
	play_se (pvsel_exit_sounds, pvsel_exit_sounds_generator);
	ctx.rip = 0x14020473B; // Skip se_ft_cansel
}

SafetyHookMid cs_pvgame_enter_hook;
void
cs_pvgame_enter (SafetyHookContext &ctx) {
	if (pvgame_enter_sounds.empty ()) return;

	auto load_data           = (u64)GetPvLoadData ();
	auto module              = *(i32 *)(load_data + 0xA8);
	std::optional<i32> chara = std::nullopt;
	for (auto it = modules->begin (); it != modules->end (); it++) {
		if (it->id == module) {
			chara = std::optional (it->chara);
			break;
		}
	}

	play_se (pvgame_enter_sounds, pvgame_enter_sounds_generator, true, chara, module);
	ctx.rip = 0x14020A968; // Skip se_ft_music_selector_enter_01
}

SafetyHookMid adv_logo_sega_hook;
void
adv_logo_sega (SafetyHookContext &ctx) {
	if (logo_sega_sounds.empty ()) return;
	ctx.rip = 0x1401A93A9;
}

bool sega_se_played     = false;
bool crypton_se_played  = false;
bool piapro_se_played   = false;
bool megastar_se_played = false;
FUNCTION_PTR (f32, GetAetMarker, 0x1402CA170, i32 sceneId, const char *layerName, const char *marker);
SafetyHookMid adv_logo_all_hook;
void
adv_logo_all (SafetyHookContext &ctx) {
	auto current_time = ctx.xmm0.f32[0];
	auto sega         = GetAetMarker (0x52A, "logo_all", "sega");
	auto crypton      = GetAetMarker (0x52A, "logo_all", "crypton");
	auto piapro       = GetAetMarker (0x52A, "logo_all", "piapro");
	auto megastar     = GetAetMarker (0x52A, "logo_all", "megastar");

	if (current_time < sega) {
		sega_se_played     = false;
		crypton_se_played  = false;
		piapro_se_played   = false;
		megastar_se_played = false;
	}

	if (current_time >= sega + 57.0 && !sega_se_played) {
		sega_se_played = true;
		play_se (logo_sega_sounds, logo_sega_sounds_generator);
	} else if (current_time >= crypton && !crypton_se_played) {
		crypton_se_played = true;
		play_se (logo_crypton_sounds, logo_crypton_sounds_generator);
	} else if (current_time >= piapro && !piapro_se_played) {
		piapro_se_played = true;
		play_se (logo_piapro_sounds, logo_piapro_sounds_generator);
	} else if (current_time >= megastar && !megastar_se_played) {
		megastar_se_played = true;
		play_se (logo_megastar_sounds, logo_megastar_sounds_generator);
	}
}

SafetyHookMid adv_title_init_hook;
void
adv_title_init (SafetyHookContext &ctx) {
	play_se (logo_title_sounds, logo_title_sounds_generator);
}

SafetyHookMid adv_title_dest_hook;
void
adv_title_dest (SafetyHookContext &ctx) {
	SoundEffectRelease (1, playing_se_name.c_str (), false);
}

SafetyHookMid pvgame_loop_hook;
std::chrono::time_point<std::chrono::high_resolution_clock> last_low_health_se;
void
pvgame_loop (SafetyHookContext &ctx) {
	auto data   = (u64)GetPvGameData ();
	auto combo  = *(u32 *)(data + 0x2D248);
	auto health = *(u32 *)(data + 0x2D234);

	auto load_data           = (u64)GetPvLoadData ();
	auto module              = *(i32 *)(load_data + 0xA8);
	std::optional<i32> chara = std::nullopt;
	for (auto it = modules->begin (); it != modules->end (); it++) {
		if (it->id == module) {
			chara = std::optional (it->chara);
			break;
		}
	}

	if (health < 64 && last_low_health_se + std::chrono::seconds (30) >= std::chrono::high_resolution_clock::now ()) {
		play_se (pvgame_low_health_sounds, pvgame_low_health_sounds_generator, false, chara, module);
		last_low_health_se = std::chrono::high_resolution_clock::now ();
	}

	if (combo > 0 && (combo % 50) == 0) play_se (pvgame_fifty_combo_sounds, pvgame_fifty_combo_sounds_generator, false, chara, module);
}

// TODO: Character filters
extern "C" {
__declspec (dllexport) void
init () {
	freopen ("CONOUT$", "w", stdout);
	av_log_set_level (AV_LOG_ERROR);

	sound_stream_array_ctrl_hook = safetyhook::create_mid (0x1405AED39, sound_stream_array_ctrl);
	data_init_hook               = safetyhook::create_mid (0x14F00D160, data_init);
	pvgame_set_rank_hook         = safetyhook::create_mid (0x14024BA9F, pvgame_set_rank);
	nswgam_pvsel_enter_hook      = safetyhook::create_mid (0x1406ED9D0, nswgam_pvsel_enter);
	nswgam_pvsel_exit_hook       = safetyhook::create_mid (0x1406F032A, nswgam_pvsel_exit);
	nswgam_pvgame_enter_hook     = safetyhook::create_mid (0x1406F8C6A, nswgam_pvgame_enter);
	cs_pvsel_enter_hook          = safetyhook::create_mid (0x140202D50, cs_pvsel_enter);
	cs_pvsel_exit_hook           = safetyhook::create_mid (0x140204712, cs_pvsel_exit);
	cs_pvgame_enter_hook         = safetyhook::create_mid (0x14020A954, cs_pvgame_enter);
	adv_logo_sega_hook           = safetyhook::create_mid (0x1401A9398, adv_logo_sega);
	adv_logo_all_hook            = safetyhook::create_mid (0x1401A9250, adv_logo_all);
	adv_title_init_hook          = safetyhook::create_mid (0x1401A9A80, adv_title_init);
	adv_title_dest_hook          = safetyhook::create_mid (0x1401AA010, adv_title_dest);
	pvgame_loop_hook             = safetyhook::create_mid (0x1405DA060, pvgame_loop);
}
}
