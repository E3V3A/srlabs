#include "session.h"
#include "output.h"
#include "bit_func.h"
#include "sms.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <osmocom/gsm/gsm_utils.h>

#ifdef USE_MYSQL
#include "mysql_api.h"
#endif

#ifdef USE_SQLITE
#include "sqlite_api.h"
#endif

#define APPEND(log, msg) snprintf(log+strlen(log), sizeof(log)-strlen(log), "%s", msg);

#ifndef MSG_VERBOSE
#define MSG_VERBOSE 0
#endif /* !MSG_VERBOSE */

uint8_t privacy = 0;
uint8_t msg_verbose = MSG_VERBOSE;
uint8_t auto_reset = 1;

#ifdef USE_AUTOTIME
	uint8_t auto_timestamp = 1;
#else
	uint8_t auto_timestamp = 0;
#endif

static uint8_t output_console = 1;
static uint8_t output_gsmtap = 1;
static uint8_t output_sqlite = 1;

static uint32_t s_id = 0;
static struct session_info *s_pointer = 0;
pthread_mutex_t s_mutex = PTHREAD_MUTEX_INITIALIZER;

struct session_info _s[2];

uint32_t now = 0;

static void console_callback(const char *sql)
{
	assert(sql != NULL);

	printf("SQL: %s", sql);
	fflush(stdout);
}

void session_init(unsigned start_sid, int console, const char *gsmtap_target, int callback)
{
	output_console = console;
	output_gsmtap = (gsmtap_target == NULL ? 0 : 1);

	// Reset both domains
	memset(_s, 0, sizeof(_s));

	switch (callback) {
	case CALLBACK_NONE:
		break;
#ifdef USE_MYSQL
	case CALLBACK_MYSQL:
		output_sqlite = 0;
		mysql_api_init(&_s[0]);
		mysql_api_init(&_s[1]);
		break;
#endif
#ifdef USE_SQLITE
	case CALLBACK_SQLITE:
		sqlite_api_init(&_s[0]);
		sqlite_api_init(&_s[1]);
		break;
#endif
	case CALLBACK_CONSOLE:
		_s[0].sql_callback = console_callback;
		_s[1].sql_callback = console_callback;
		break;
	}

	s_id = start_sid;

	_s[0].id = s_id++;
	_s[1].id = s_id++;
	_s[1].domain = DOMAIN_PS;

	if (gsmtap_target != NULL)
	{
		net_init(gsmtap_target);
	}
}

void session_destroy(unsigned *last_sid, unsigned *last_cid)
{
	if (msg_verbose > 1) {
		printf("session_destroy!\n");
	}

	session_reset(&_s[0], 0);
	_s[1].new_msg = NULL;
	session_reset(&_s[1], 0);
	*last_sid = s_id;

	cell_destroy(last_cid);
	net_destroy();

	if (_s[0].sql_callback) {
#ifdef USE_SQLITE
		if (output_sqlite == 1) {
			sqlite_api_destroy();
		}
#endif
#ifdef USE_MYSQL
		if (output_sqlite == 0) {
			mysql_api_destroy();
		}
#endif
	}
}

struct session_info *session_create(int id, char* name, uint8_t *key, int mcc, int mnc, int lac, int cid, struct gsm_sysinfo_freq *ca)
{
	struct session_info *ns;

	ns = (struct session_info *) malloc(sizeof(struct session_info));
	memset(ns, 0, sizeof(struct session_info));

	if (id < 0) {
		ns->id = s_id++; 
	} else {
		ns->id = id;
	}

	if (name) {
		strcpy(ns->name, name);
	}

	/* Set timestamp */
	if (auto_timestamp) {
		gettimeofday(&ns->timestamp, 0);
	} else {
		ns->timestamp.tv_sec  = now;
		ns->timestamp.tv_usec = 0;
	}

	if (key) {
		ns->have_key = 1;
		memcpy(ns->key, key, 8);
	}

	/* Copy cell ID */
	ns->mcc = mcc;
	ns->mnc = mnc;
	ns->lac = lac;
	ns->cid = cid;

	/* Store cell ARFCNs */
	if (ca)
		memcpy(ns->cell_arfcns, ca, 1024*sizeof(struct gsm_sysinfo_freq));

	ns->decoded = 1;
	rand_init_2b(&ns->null);
	rand_init_2b(&ns->other_sdcch);
	rand_init_2b(&ns->other_sacch);

	pthread_mutex_lock(&s_mutex);

	if (s_pointer)
		s_pointer->prev = ns;
	ns->next = s_pointer;
	s_pointer = ns;

	pthread_mutex_unlock(&s_mutex);

	return ns;
}

int session_enumerate(int output)
{
	struct session_info *s;
	int count;

	s = s_pointer;
	count = 0;

	if (output)
		printf("Open sessions:\n");

	while (s) {
		if (s->processing) {
			if (output)
				printf(" %d: %s\n", s->id, s->name);
			count++;
		}
		s = s->next;
	}

	printf("\n");
	fflush(stdout);

	return count;
}

void session_free_msg_list(struct session_info *s)
{
	struct radio_message *m;

	assert(s != NULL);

	while (s->first_msg) {
		m = s->first_msg;
		if (msg_verbose > 1) {
			printf("Freeing pointer %p\n", m);
		}
		s->first_msg = m->next;

		free(m);
	}
}

void session_free_sms_list(struct session_info *s)
{
	struct sms_meta *sm;

	assert(s != NULL);

	while (s->sms_list) {
		sm = s->sms_list;

		s->sms_list = sm->next;

		free(sm);
	}
}

void session_free(struct session_info *s)
{
	assert(auto_reset == 0);
	assert(s != NULL);

	if (s->prev) {
		s->prev->next = s->next;
	}
	if (s->next) {
		s->next->prev = s->prev;
	}
	if (s_pointer == s) {
		s_pointer = s->next;
	}

	session_free_msg_list(s);
	session_free_sms_list(s);
	free(s);
}

void session_stream(struct session_info *s)
{
	struct radio_message *m;

	assert(s != NULL);

	m = s->first_msg;
	while (m) {
		if (m->flags & MSG_DECODED) {
			net_send_msg(m);
#if 0
			if (msg_verbose && m->info[0]) {
				printf("%c %s\n", m->bb.arfcn[0] & ARFCN_UPLINK ? 'U' : 'D', m->info);
			}
#endif
		}
		m = m->next;
	}
}

void session_print(struct session_info *s)
{
	char log[4096];
	char strbuf[256];

	assert(s != NULL);

	if (!s->started)
		return;

	/* Init log */
	log[0] = 0;

	snprintf(strbuf, 256, "\nmcc %d mnc %d lac %d cid %d\n", s->mcc, s->mnc, s->lac, s->cid); 
	APPEND(log, strbuf);
	
	/* Cipher & Integrity */
	switch (s->rat) {
	case RAT_GSM:
		snprintf(strbuf, 256, "cipher: A5/%d\n", s->cipher); 
		break;
	case RAT_UMTS:
		snprintf(strbuf, 256, "cipher: UEA/%d\nintegrity: UIA/%d\n", s->cipher, s->integrity); 
		break;
	case RAT_LTE:
		snprintf(strbuf, 256, "cipher: EEA/%d\nintegrity: EIA/%d\n", s->cipher, s->integrity); 
		break;
	}	
	APPEND(log, strbuf);

	if (s->cipher) {
		if (s->decoded == 1) {
			hex_bin2str(s->key, strbuf, 8);
			strbuf[16] = 0;
			APPEND(log, "key: "); 
			APPEND(log, strbuf);
			APPEND(log, "\n");
		} else if (s->decoded < 0) {
			APPEND(log, "key: not found\n"); 
		} else {
			APPEND(log, "key: not available\n"); 
		}
	} else {
		APPEND(log, "key: -\n"); 
	}

	/* Randomization */
	APPEND(log, "randomization:");
	if (s->si5.byte_count) {
		snprintf(strbuf, 256, " si5 %d%%", 100*s->si5.rand_count/s->si5.byte_count);
		APPEND(log, strbuf);
	}
	if (s->si5bis.byte_count) {
		snprintf(strbuf, 256, " si5bis %d%%", 100*s->si5bis.rand_count/s->si5bis.byte_count);
		APPEND(log, strbuf);
	}
	if (s->si5ter.byte_count) {
		snprintf(strbuf, 256, " si5ter %d%%", 100*s->si5ter.rand_count/s->si5ter.byte_count);
		APPEND(log, strbuf);
	}
	if (s->si6.byte_count) {
		snprintf(strbuf, 256, " si6 %d%%", 100*s->si6.rand_count/s->si6.byte_count);
		APPEND(log, strbuf);
	}
	if (s->null.byte_count) {
		snprintf(strbuf, 256, " null %d%%", 100*s->null.rand_count/s->null.byte_count);
		APPEND(log, strbuf);
	}
	if (s->other_sdcch.byte_count) {
		snprintf(strbuf, 256, " sdcch_pad %d%%", 100*s->other_sdcch.rand_count/s->other_sdcch.byte_count);
		APPEND(log, strbuf);
	}
	if (s->other_sacch.byte_count) {
		snprintf(strbuf, 256, " sacch_pad %d%%", 100*s->other_sacch.rand_count/s->other_sacch.byte_count);
		APPEND(log, strbuf);
	}
	APPEND(log, "\n");

	/* Transaction details */
	APPEND(log, "report:");
	if (s->mo)
		APPEND(log, " mo");
	if (s->mt)
		APPEND(log, " mt");
	if (s->locupd)
		APPEND(log, " locupd");
	if (s->call)
		APPEND(log, " call");
	if (s->sms)
		APPEND(log, " sms");
	if (s->ssa)
		APPEND(log, " ssa");
	if (s->detach)
		APPEND(log, " detach");
	if (s->auth)
		APPEND(log, " auth");
	if (s->iden_imsi_ac || s->iden_imsi_bc)
		APPEND(log, " iden_imsi");
	if (s->iden_imei_ac || s->iden_imei_bc)
		APPEND(log, " iden_imei");
	if (s->tmsi_realloc)
		APPEND(log, " tmsi_realloc");
	if (s->assignment)
		APPEND(log, " assignment");
	if (s->handover)
		APPEND(log, " handover");
	if (s->have_gprs)
		APPEND(log, " gprs");
	if (s->release)
		APPEND(log, " release");
	if (s->cipher && !s->cmc_imeisv)
		APPEND(log, " no_imeisv");
	if (s->forced_ho)
		APPEND(log, "\nFORCED HANDOVER!");
	if (s->assignment || s->handover) {
		snprintf(strbuf, 256, "\nchan mode: %02x rate conf: %02x", s->ga.chan_mode, s->ga.rate_conf);
		APPEND(log, strbuf);
	}
	if (s->ms_cipher_mask) {
		APPEND(log, "\nMS ciphers:");
		if ((s->ms_cipher_mask & 1) || (s->cipher == 1))
			APPEND(log, " A5/1");
		if ((s->ms_cipher_mask & 2) || (s->cipher == 2))
			APPEND(log, " A5/2");
		if ((s->ms_cipher_mask & 4) || (s->cipher == 3))
			APPEND(log, " A5/3");
	}
	if (s->ue_cipher_cap) {
		APPEND(log, "\nUE ciphers:");
		if ((s->ue_cipher_cap & 1) || (s->cipher == 1))
			APPEND(log, " UEA/1");
		if ((s->ue_cipher_cap & 2) || (s->cipher == 2))
			APPEND(log, " UEA/2");
		if ((s->ue_cipher_cap & 3) || (s->cipher == 3))
			APPEND(log, " UEA/2");
	}
	APPEND(log, "\navailable IDs:");
	if (not_zero(s->old_tmsi, 4)) {
		hex_bin2str(s->old_tmsi, strbuf, 4);
		strbuf[8] = 0;
		APPEND(log, " TMSI=");
		APPEND(log, strbuf);
	}
	if (not_zero(s->new_tmsi, 4)) {
		hex_bin2str(s->new_tmsi, strbuf, 4);
		strbuf[8] = 0;
		APPEND(log, " NEW_TMSI=");
		APPEND(log, strbuf);
	}
	if (not_zero(s->tlli, 0)) {
		hex_bin2str(s->tlli, strbuf, 4);
		strbuf[8] = 0;
		APPEND(log, " TLLI=");
		APPEND(log, strbuf);
	}
	if (privacy == 0) {
		if (strlen(s->imsi)) {
			APPEND(log, " IMSI=");
			APPEND(log, s->imsi);
		}
		if (strlen(s->imei)) {
			APPEND(log, " IMEI=");
			APPEND(log, s->imei);
		}
		if (strlen(s->msisdn)) {
			APPEND(log, " MSISDN=");
			APPEND(log, s->msisdn);
		}
	}

	printf("%s\n\n", log);
	fflush(stdout);
}

void session_make_sql(struct session_info *s, char *query, unsigned q_len, uint8_t sqlite)
{
	char timestamp[40];
	char *tmsi;
	char *new_tmsi;
	char *tlli;
	char *hash;
	char *imsi;
	char *imei;
	char *msisdn;
	char *pdpip;
	char id_field[4];
	char id_value[16];

	assert(s != NULL);
	assert(query != NULL);

	if (!s->started)
		return;

	if (s->closed)
		return;

	/* Prepare strings */
	if (s->id >= 0) {
		strncpy(id_field, "id,", sizeof(id_field));
		snprintf(id_value, sizeof(id_value), "%d,", s->id);
	} else {
		id_field[0] = 0;
		id_value[0] = 0;
	}
	if (not_zero(s->old_tmsi,4)) {
		tmsi = strescape_or_null(osmo_hexdump_nospc(s->old_tmsi, 4));
	} else {
		tmsi = strescape_or_null(0);
	}
	if (not_zero(s->new_tmsi,4)) {
		new_tmsi = strescape_or_null(osmo_hexdump_nospc(s->new_tmsi, 4));
	} else {
		new_tmsi = strescape_or_null(0);
	}
	if (not_zero(s->tlli,4)) {
		tlli = strescape_or_null(osmo_hexdump_nospc(s->tlli, 4));
	} else {
		tlli = strescape_or_null(0);
	}
	hash = strescape_or_null(s->name);
	imsi = strescape_or_null(s->imsi);
	imei = strescape_or_null(s->imei);
	msisdn = strescape_or_null(s->msisdn);
	pdpip = strescape_or_null(s->pdp_ip);

	if (sqlite) {
		snprintf(timestamp, sizeof(timestamp), "datetime(%lu, 'unixepoch')", s->timestamp.tv_sec);
	} else {
		snprintf(timestamp, sizeof(timestamp), "FROM_UNIXTIME(%lu)", s->timestamp.tv_sec);
	}

	/* Prepare query */
	snprintf(query, q_len,
		"INSERT INTO session_info (%stimestamp,rat,domain,mcc,mnc,lac,cid,arfcn,psc,cracked,neigh_count,"
		"unenc,unenc_rand,enc,enc_rand,enc_null,enc_null_rand,enc_si,enc_si_rand,predict,"
		"avg_power,uplink_avail,initial_seq,cipher_seq,auth,auth_req_fn,auth_resp_fn,auth_delta,"
		"cipher_missing,cipher_comp_first,cipher_comp_last,cipher_comp_count,cipher_delta,cipher,"
		"integrity,cmc_imeisv,first_fn,last_fn,duration,mobile_orig,mobile_term,paging_mi,"
		"t_unknown,t_detach,t_locupd,lu_type,lu_acc,lu_reject,lu_rej_cause,lu_mcc,lu_mnc,lu_lac,"
		"t_abort,t_raupd,t_attach,att_acc,t_pdp,pdp_ip,t_call,t_sms,t_ss,"
		"t_tmsi_realloc,t_release,rr_cause,t_gprs,iden_imsi_ac,iden_imsi_bc,iden_imei_ac,iden_imei_bc,"
		"assign,assign_cmpl,handover,forced_ho,a_timeslot,a_chan_type,a_tsc,"
		"a_hopping,a_arfcn,a_hsn,a_maio,a_ma_len,a_chan_mode,a_multirate,"
		"call_presence,sms_presence,service_req,"
		"imsi,imei,tmsi,new_tmsi,tlli,msisdn,"
		"ms_cipher_mask,ue_cipher_cap,ue_integrity_cap) VALUES "
		"(%s%s,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
		"%d,%d,%d,%d,%d,%d,%d,%d,%d,"
		"%d,%d,%d,%d,%d,%d,%d,%d,"
		"%d,%d,%d,%d,%d,%d,"
		"%d,%d,%d,%d,%d,%d,%d,%d,"
		"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,"
		"%d,%d,%d,%d,%d,%s,%d,%d,%d,"
		"%d,%d,%d,%d,%d,%d,%d,%d,"
		"%d,%d,%d,%d,%d,%d,%d,"
		"%d,%d,%d,%d,%d,%d,%d,"
		"%d,%d,%d,"
		"%s,%s,%s,%s,%s,%s,"
		"%d,%d,%d);\n",
		id_field, id_value,
		timestamp, s->rat, s->domain, s->mcc, s->mnc, s->lac, s->cid, s->arfcn, s->psc, s->cracked, s->neigh_count,
		s->fc.unenc, s->fc.unenc_rand, s->fc.enc, s->fc.enc_rand, s->fc.enc_null, s->fc.enc_null_rand, s->fc.enc_si, s->fc.enc_si_rand, s->fc.predict,
		s->avg_power, s->uplink, s->initial_seq, s->cipher_seq, s->auth, s->auth_req_fn, s->auth_resp_fn, s->auth_delta,
		s->cipher_missing, s->cm_comp_first_fn, s->cm_comp_last_fn, s->cm_comp_count, s->cipher_delta, s->cipher,
		s->integrity, s->cmc_imeisv, s->first_fn, s->last_fn, s->duration, s->mo, s->mt, s->pag_mi,
		s->unknown, s->detach, s->locupd, s->lu_type, s->lu_acc, s->lu_reject, s->lu_rej_cause, s->lu_mcc, s->lu_mnc, s->lu_lac,
		s->abort, s->raupd, s->attach, s->att_acc, s->pdp_activate, pdpip, s->call, s->sms, s->ssa,
		s->tmsi_realloc, s->release, s->rr_cause, s->have_gprs, s->iden_imsi_ac, s->iden_imsi_bc, s->iden_imei_ac, s->iden_imei_bc,
		s->assignment, s->assign_complete, s->handover, s->forced_ho, s->ga.chan_nr&7, s->ga.chan_nr>>3, s->ga.tsc,
		s->ga.h, s->ga.h0.band_arfcn, s->ga.h1.hsn, s->ga.h1.maio, s->ga.h1.ma_len, s->ga.chan_mode, s->ga.rate_conf,
		s->call_presence, s->sms_presence, s->serv_req,
		imsi, imei, tmsi, new_tmsi, tlli, msisdn,
		s->ms_cipher_mask, s->ue_cipher_cap, s->ue_integrity_cap);

	/* Deallocate strings */
	free(tmsi);
	free(new_tmsi);
	free(tlli);
	free(hash);
	free(imsi);
	free(imei);
	free(msisdn);
	free(pdpip);
}

void session_close(struct session_info *s)
{
	assert(s != NULL);

	s->processing = 0;

	/* Attach or update timestamp */
	if (auto_timestamp) {
		gettimeofday(&s->timestamp, NULL);
	} else {
		if (now) {
			s->timestamp.tv_sec = now;
			s->timestamp.tv_usec = 0;
		}
	}

	/* Estimate transaction duration */
	if (s->first_fn <= s->last_fn)
		s->duration = s->last_fn - s->first_fn;
	else
		s->duration = ((s->last_fn + GSM_MAX_FN) - s->first_fn) % GSM_MAX_FN;

	s->duration *= 4.615f;

	/* Estimate authentication delta */
	if (s->auth && s->auth_req_fn && s->auth_resp_fn) {
		if (s->auth_req_fn <= s->auth_resp_fn) {
			s->auth_delta = s->auth_resp_fn - s->auth_req_fn;
		} else {
			s->auth_delta = ((s->auth_resp_fn + GSM_MAX_FN) - s->auth_req_fn) % GSM_MAX_FN;
		}
	}
	s->auth_delta *= 4.615f;

	/* Estimate cipher delta */
	if (s->cipher && s->cm_cmd_fn && s->cm_comp_last_fn) {
		if (s->cm_cmd_fn <= s->cm_comp_last_fn) {
			s->cipher_delta = s->cm_comp_last_fn - s->cm_cmd_fn;
		} else {
			s->cipher_delta = ((s->cm_comp_last_fn + GSM_MAX_FN) - s->cm_cmd_fn) % GSM_MAX_FN;
		}
	}
	s->cipher_delta *= 4.615f;

#if 0
	/* Process neighbour list */
	s->neigh_count = 0;
	for (i=0; i<1024; i++) {
		if (s->neigh_arfcns[i].mask) {
			s->neigh_count++;
		}
	}
#endif

	/* Output functions */
	if (output_gsmtap && !auto_reset)
		session_stream(s);

	if (output_console)
		session_print(s);

	if (s->sql_callback) {
		char sql_buffer[8192];
		struct sms_meta *sm;

		session_make_sql(s, sql_buffer, sizeof(sql_buffer), output_sqlite);

		s->sql_callback(sql_buffer);

		sm = s->sms_list;
		while (sm) {
			sms_make_sql(s->id, sm, sql_buffer, sizeof(sql_buffer));

			s->sql_callback(sql_buffer);

			sm = sm->next;
		}

		if (s->appid) {
			snprintf(sql_buffer, sizeof(sql_buffer),
				 "INSERT INTO sid_appid VALUES (%d,'%08x');\n", s->id, s->appid); 

			s->sql_callback(sql_buffer);
		}
	}

	s->closed = 1;
}

void link_to_msg_list(struct session_info* s, struct radio_message *m)
{
	if (msg_verbose > 1) {
		printf("linking to domain %d message ptr %p\n", s->domain, m);
	}

	if (s->first_msg == NULL) {
		s->first_msg = m;
	}

	if (s->last_msg) {
		assert(s->last_msg->next == NULL);
		s->last_msg->next = m;
	}
	m->next = NULL;
	m->prev = s->last_msg;
	s->last_msg = m;
}

void session_reset(struct session_info *s, int forced_release)
{
	struct session_info old_s;
	struct radio_message *m = NULL;

	if (auto_reset == 0) {
		return;
	}
	if (msg_verbose > 1) {
		printf("Session RESET! domain: %d, forced release: %d\n", s->domain, forced_release);
	}

	assert(s != NULL);

	//Detaching the last attached message to the session.
	if (forced_release) {
		//assert(s->new_msg);
		m = s->new_msg;
	} else {
		if (s->new_msg) { //&& (s->new_msg->flags & MSG_DECODED)
			link_to_msg_list(s, s->new_msg);
		}
		s->new_msg = NULL;
	}

	if (s->started && !s->closed) {
		switch (s->rat) {
		case RAT_GSM:
			printf("RAT: GSM\n");
			break;
		case RAT_UMTS:
			printf("RAT: 3G\n");
			break;
		case RAT_LTE:
			printf("RAT: LTE\n");
			break;
		default:
			printf("RAT: UNKNOWN\n");
		}
		fflush(stdout);
		s->cracked = 1;
		session_close(s);
	}

	memcpy(&old_s, s, sizeof(struct session_info));

	//Set up 's'
	memset(s, 0, sizeof(struct session_info));
	if (old_s.started && old_s.closed) {
		s->id = ++s_id;
	} else {
		s->id = old_s.id;
	}
	s->appid = old_s.appid;
	strncpy(s->name, old_s.name, sizeof(s->name));
	s->domain = old_s.domain;
	if (!auto_timestamp) {
		s->timestamp = old_s.timestamp;
	}
	s->mcc = old_s.mcc;
	s->mnc = old_s.mnc;
	s->lac = old_s.lac;
	if (old_s.rat != RAT_GSM) {
		s->cid = old_s.cid;
	}
	if (strlen(old_s.imsi)) {
		strncpy(s->imsi, old_s.imsi, sizeof(s->imsi));
	}
	s->sql_callback = old_s.sql_callback;

	if (forced_release) {
		s->new_msg = m;
	}

	/* Copy information for repeated message detection */
	if (old_s.last_dtap_len) {
		s->last_dtap_len = old_s.last_dtap_len;
		memcpy(s->last_dtap, old_s.last_dtap, old_s.last_dtap_len); 
		s->last_dtap_rat = old_s.last_dtap_rat;
	}

	/* Copy temporary message buffers for GSM */
	memcpy(s->chan_sdcch, old_s.chan_sdcch, sizeof(s->chan_sdcch));
	memcpy(s->chan_sacch, old_s.chan_sacch, sizeof(s->chan_sdcch));
	memcpy(s->chan_facch, old_s.chan_facch, sizeof(s->chan_sdcch));

	/* Free allocated memory */

	//TODO remove the check below, it's *expensive*
	if (msg_verbose > 1) {
		printf("session reset (at the end of the function), domain: %d\n", old_s.domain);
	}
	struct radio_message *tmp = old_s.first_msg;
	while (tmp) {
		assert(tmp != m);
		tmp = tmp->next;
	}

	session_free_msg_list(&old_s);
	session_free_sms_list(&old_s);
	old_s.first_msg = NULL;
	old_s.last_msg = NULL;
}

static uint32_t parse_appid(const char *filename)
{
	char *fn_copy;
	char *ptr;
	char *token;
	uint32_t appid;

	/* We need a copy, tokenizer is not const */
	fn_copy = strdup(filename);

	/* Match file name header */
	ptr = strstr(fn_copy, "2__");
	if (!ptr) {
		return 0;
	}

	/* Skip first part */
	ptr += 3;

	/* Get and ignore first token */
	token = strtok_r(ptr, "_", &ptr);
	if (!token) {
		return 0;
	}

	/* Match App ID string */
	token = strtok_r(0, "_", &ptr);
	if (strlen(token) != 8) {
		return 0;
	}

	/* Parse and return value */
	if (sscanf(token, "%08x", &appid) == 1) {
		return appid;
	}

	return 0;
}

int session_from_filename(const char *filename, struct session_info *s)
{
	char *xgs_ptr;
	char *qdmon_ptr;
	char *ptr;
	char *token;
	unsigned mcc_mnc;
	struct tm ts;
	struct timeval now;
	int ret;

	/* Try to extract application ID */
	s->appid = parse_appid(filename);

	/* Locate baseband type in filename */
	xgs_ptr = strstr(filename, "_xgs.");
	qdmon_ptr = strstr(filename, "_qdmon.");

	/* Only one string should match */
	if (xgs_ptr) {
		if (qdmon_ptr) {
			goto parse_error;
		} else {
			ptr = xgs_ptr;
		}
	} else {
		if (qdmon_ptr) {
			ptr = qdmon_ptr;
		} else {
			goto parse_error;
		}
	}

	/* Create tokenizer and skip first element */
	token = strtok_r(ptr, ".", &ptr);
	if (!token)
		goto parse_error;

	/* Get phone model (needed for xgs only) */
	token = strtok_r(0, ".", &ptr);
	if (!token) {
		goto parse_error;
	} else {
		// Do model checks for xgs, not really needed for now
	}

	/* Next token */
	token = strtok_r(0, ".", &ptr);
	if (!token)
		goto parse_error;

	ret = strlen(token);

	/* Some model might include version with a dot */
	if (ret == 1 || ret == 2) {
		/* Advance to next token */
		token = strtok_r(0, ".", &ptr);
		if (!token)
			goto parse_error;

		ret = strlen(token);
	}

	/* Check if filename has new IMSI field */
	if (ret == 5 || ret == 6) {
		/* Check if actual MCC/MNC values are present */
		if (sscanf(token, "%06d", &mcc_mnc) == 1) {
			/* Save them as part of IMSI */
			strncpy(s->imsi, token, sizeof(s->imsi));
		}

		/* Advance to next token */
		token = strtok_r(0, ".", &ptr);
		if (!token)
			goto parse_error;
	}

	gettimeofday(&now, NULL);
	memset(&ts, 0, sizeof(ts));

	/* Timestamp */
	ret = sscanf(token, "%04d%02d%02d-%02d%02d%02d",
			&ts.tm_year, &ts.tm_mon, &ts.tm_mday,
			&ts.tm_hour, &ts.tm_min, &ts.tm_sec);
	if (ret != 6) {
		fprintf(stderr, "unknown timestamp format %s\n", (token?token:"(null)"));
		goto parse_error;
	} else {
		ts.tm_year -= 1900;
		ts.tm_mon -= 1;
		s->timestamp.tv_sec = mktime(&ts);
		/* Allow timestamps with 12h in advance */
		if (s->timestamp.tv_sec > (now.tv_sec + 43200)) {
			s->timestamp = now;
			fprintf(stderr, "timestamp %s is in the future! using current timestamp\n", token);
		}
	}

	/* Network type */
	token = strtok_r(0, ".", &ptr);
	if (!token)
		goto parse_error;

	if (!strcmp(token, "UMTS") ||
	    !strcmp(token, "3G")||
	    !strcmp(token, "WCDMA")) {
		s->rat = RAT_UMTS;
	} else if (!strcmp(token, "GSM") ||
		   !strcmp(token, "UNKNOWN") ||
		   !strcmp(token, "UNKNWON") ||
		   !strcmp(token, "null")) {
		s->rat = RAT_GSM;
	} else if (!strcmp(token, "LTE")) {
		s->rat = RAT_LTE;
	} else {
		// unknown
		fprintf(stderr, "unknown network type %s\n", token);
		goto parse_error;
	}

	/* Cell ID */
	token = strtok_r(0, ".", &ptr);
	if (!token)
		goto parse_error;

	ret = sscanf(token, "%03hu%03hu-%hx-%x", &s->mcc, &s->mnc, &s->lac, &s->cid);
	if (ret < 4) {
		/* Sometimes LAC/CID is set to "null" */
		s->lac = 65535;
		s->cid = 65535;

		if (ret < 2) {
			/* We couldn't parse even the MCC/MNC */
			fprintf(stderr, "unknown cellid format %s\n", token);
			s->mcc = 65535;
			s->mnc = 65535;
			goto parse_error;
		}
	}

	return 0;

parse_error:
	if (auto_timestamp) {
		gettimeofday(&s->timestamp, NULL);
	}
	return -1;
}
