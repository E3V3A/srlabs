/*!40101 SET storage_engine=MyISAM */;

-- operators to be listed
drop view if exists valid_op;
drop table if exists valid_op;
create table valid_op (
	mcc SMALLINT UNSIGNED NOT NULL,
	mnc SMALLINT UNSIGNED NOT NULL,
	country CHAR(32) NOT NULL,
	network CHAR(32) NOT NULL,
	oldest DATE NOT NULL,
	latest DATE NOT NULL,
	cipher TINYINT UNSIGNED NOT NULL,
	PRIMARY KEY (mcc,mnc,country,network,cipher)
);

-- operator risk, main score (level 1)
drop table if exists risk_category;
create table risk_category (
	mcc SMALLINT UNSIGNED NOT NULL,
	mnc SMALLINT UNSIGNED NOT NULL,
	lac SMALLINT UNSIGNED NOT NULL,
	month CHAR(7) NOT NULL,
	intercept FLOAT(1),
	impersonation FLOAT(1),
	tracking FLOAT(1),
	PRIMARY KEY (mcc,mnc,lac,month)
);

-- operator risk, intercept sub-score (level 2)
drop table if exists risk_intercept;
create table risk_intercept (
	mcc SMALLINT UNSIGNED NOT NULL,
	mnc SMALLINT UNSIGNED NOT NULL,
	lac SMALLINT UNSIGNED NOT NULL,
	month CHAR(7) NOT NULL,
	voice FLOAT(1),
	sms FLOAT(1),
	PRIMARY KEY (mcc,mnc,lac,month)
);

-- operator risk, impersonation sub-score (level 2)
drop table if exists risk_impersonation;
create table risk_impersonation (
	mcc SMALLINT UNSIGNED NOT NULL,
	mnc SMALLINT UNSIGNED NOT NULL,
	lac SMALLINT UNSIGNED NOT NULL,
	month CHAR(7) NOT NULL,
	make_calls FLOAT(1),
	recv_calls FLOAT(1),
	PRIMARY KEY (mcc,mnc,lac,month)
);

-- operator risk, tracking  sub-score (level 2)
drop table if exists risk_tracking;
create table risk_tracking (
	mcc SMALLINT UNSIGNED NOT NULL,
	mnc SMALLINT UNSIGNED NOT NULL,
	lac SMALLINT UNSIGNED NOT NULL,
	month CHAR(7) NOT NULL,
	local_track FLOAT(1),
	global_track FLOAT(1),
	PRIMARY KEY (mcc,mnc,lac,month)
);

-- operator risk, attack components (level 3)
drop table if exists attack_component;
create table attack_component (
	mcc SMALLINT UNSIGNED NOT NULL,
	mnc SMALLINT UNSIGNED NOT NULL,
	lac SMALLINT UNSIGNED NOT NULL,
	month CHAR(7) NOT NULL,
	realtime_crack FLOAT(1),
	offline_crack FLOAT(1),
	key_reuse_mt FLOAT(1),
	key_reuse_mo FLOAT(1),
	track_tmsi FLOAT(1),
	hlr_inf FLOAT(1),
	freq_predict FLOAT(1),
	PRIMARY KEY (mcc,mnc,lac,month)
);

drop table if exists attack_component_x4;
create table attack_component_x4 (
	mcc SMALLINT UNSIGNED NOT NULL,
	mnc SMALLINT UNSIGNED NOT NULL,
	lac SMALLINT UNSIGNED NOT NULL,
	month CHAR(7) NOT NULL,
	cipher SMALLINT UNSIGNED,
	call_perc FLOAT(1),
	sms_perc FLOAT(1),
	loc_perc FLOAT(1),
	realtime_crack FLOAT(1),
	offline_crack FLOAT(1),
	key_reuse_mt FLOAT(1),
	key_reuse_mo FLOAT(1),
	track_tmsi FLOAT(1),
	hlr_inf FLOAT(1),
	freq_predict FLOAT(1),
	PRIMARY KEY (mcc,mnc,lac,month,cipher)
);

-- operator security metrics (level 4)
drop table if exists sec_params;
create table sec_params (
	mcc SMALLINT UNSIGNED NOT NULL,
	mnc SMALLINT UNSIGNED NOT NULL,
	country CHAR(32) NOT NULL,
	network CHAR(32) NOT NULL,
	lac SMALLINT UNSIGNED NOT NULL,
	month CHAR(7) NOT NULL,
	cipher SMALLINT UNSIGNED NOT NULL,
	call_count INTEGER UNSIGNED,
	call_mo_count INTEGER UNSIGNED,
	sms_count INTEGER UNSIGNED,
	sms_mo_count INTEGER UNSIGNED,
	loc_count INTEGER UNSIGNED,
	call_success REAL,
	sms_success REAL,
	loc_success REAL,
	call_null_rand REAL,
	sms_null_rand REAL,
	loc_null_rand REAL,
	call_si_rand REAL,
	sms_si_rand REAL,
	loc_si_rand REAL,
	call_nulls REAL,
	sms_nulls REAL,
	loc_nulls REAL,
	call_pred REAL,
	sms_pred REAL,
	loc_pred REAL,
	call_imeisv REAL,
	sms_imeisv REAL,
	loc_imeisv REAL,
	pag_auth_mt REAL,
	call_auth_mo REAL,
	sms_auth_mo REAL,
	loc_auth_mo REAL,
	call_tmsi REAL,
	sms_tmsi REAL,
	loc_tmsi REAL,
	call_imsi REAL,
	sms_imsi REAL,
	loc_imsi REAL,
	ma_len REAL,
	var_len REAL,
	var_hsn REAL,
	var_maio REAL,
	var_ts REAL,
	rand_imsi REAL,
	home_routing REAL,
	PRIMARY KEY (mcc,mnc,lac,month,cipher)
);

-- auxiliary table for call parameters
drop view if exists call_avg;
drop table if exists call_avg;
create table call_avg (
	mcc SMALLINT UNSIGNED NOT NULL,
	mnc SMALLINT UNSIGNED NOT NULL,
	lac SMALLINT UNSIGNED NOT NULL,
	month CHAR(7) NOT NULL,
	cipher SMALLINT UNSIGNED NOT NULL,
	count INTEGER UNSIGNED,
	mo_count INTEGER UNSIGNED,
	success FLOAT(1),
	rand_null_perc FLOAT(1),
	rand_si_perc FLOAT(1),
	nulls FLOAT(1),
	pred FLOAT(1),
	imeisv FLOAT(1),
	auth_mt FLOAT(1),
	auth_mo FLOAT(1),
	tmsi FLOAT(1),
	imsi FLOAT(1),
	PRIMARY KEY (mcc,mnc,lac,month,cipher)
);

-- auxiliary table for SMS parameters
drop view if exists sms_avg;
drop table if exists sms_avg;
create table sms_avg (
	mcc SMALLINT UNSIGNED NOT NULL,
	mnc SMALLINT UNSIGNED NOT NULL,
	lac SMALLINT UNSIGNED NOT NULL,
	month CHAR(7) NOT NULL,
	cipher SMALLINT UNSIGNED NOT NULL,
	count INTEGER UNSIGNED,
	mo_count INTEGER UNSIGNED,
	success FLOAT(1),
	rand_null_perc FLOAT(1),
	rand_si_perc FLOAT(1),
	nulls FLOAT(1),
	pred FLOAT(1),
	imeisv FLOAT(1),
	auth_mt FLOAT(1),
	auth_mo FLOAT(1),
	tmsi FLOAT(1),
	imsi FLOAT(1),
	PRIMARY KEY (mcc,mnc,lac,month,cipher)
);

-- auxiliary table for LUR parameters
drop view if exists loc_avg;
drop table if exists loc_avg;
create table loc_avg (
	mcc SMALLINT UNSIGNED NOT NULL,
	mnc SMALLINT UNSIGNED NOT NULL,
	lac SMALLINT UNSIGNED NOT NULL,
	month CHAR(7) NOT NULL,
	cipher SMALLINT UNSIGNED NOT NULL,
	count INTEGER UNSIGNED,
	mo_count INTEGER UNSIGNED,
	success FLOAT(1),
	rand_null_perc FLOAT(1),
	rand_si_perc FLOAT(1),
	nulls FLOAT(1),
	pred FLOAT(1),
	imeisv FLOAT(1),
	auth_mt FLOAT(1),
	auth_mo FLOAT(1),
	tmsi FLOAT(1),
	imsi FLOAT(1),
	PRIMARY KEY (mcc,mnc,lac,month,cipher)
);

-- auxiliary table for cell entropy parameters
drop view if exists en;
drop table if exists entropy_cell;
create table entropy_cell (
	mcc SMALLINT UNSIGNED NOT NULL,
	mnc SMALLINT UNSIGNED NOT NULL,
	lac SMALLINT UNSIGNED NOT NULL,
	cid SMALLINT UNSIGNED NOT NULL,
	month CHAR(7) NOT NULL,
	cipher SMALLINT UNSIGNED NOT NULL,
	a_len FLOAT(1),
	v_len FLOAT(1),
	v_hsn FLOAT(1),
	v_maio FLOAT(1),
	v_ts FLOAT(1),
	v_tsc FLOAT(1),
	PRIMARY KEY (mcc,mnc,lac,cid,month,cipher)
);

-- auxiliary table for LAC entropy parameters
drop view if exists e;
drop table if exists entropy;
create table entropy (
	mcc SMALLINT UNSIGNED NOT NULL,
	mnc SMALLINT UNSIGNED NOT NULL,
	lac SMALLINT UNSIGNED NOT NULL,
	month CHAR(7) NOT NULL,
	cipher SMALLINT UNSIGNED NOT NULL,
	ma_len FLOAT(1),
	var_len FLOAT(1),
	var_hsn FLOAT(1),
	var_maio FLOAT(1),
	var_ts FLOAT(1),
	var_tsc FLOAT(1),
	PRIMARY KEY (mcc,mnc,lac,month,cipher)
);

--  3G risk table
DROP TABLE IF EXISTS risk_3G;
CREATE TABLE risk_3G (
	mcc SMALLINT UNSIGNED NOT NULL,
	mnc SMALLINT UNSIGNED NOT NULL,
	month CHAR(7) NOT NULL,
	lac SMALLINT UNSIGNED NOT NULL,
	total_samples INTEGER UNSIGNED,
	intercept_samples INTEGER UNSIGNED,
	impersonation_samples INTEGER UNSIGNED,
	call_perc FLOAT(1),
	sms_perc FLOAT(1),
	lu_perc FLOAT(1),
	enc_perc FLOAT(1),
	enc_lu_perc FLOAT(1),
	auth_mo_perc FLOAT(1),
	auth_mt_perc FLOAT(1),
	auth_aka_perc FLOAT(1),
	tmsi_realloc_perc FLOAT(1),
	intercept3G FLOAT(1),
	impersonation3G FLOAT(1),
	PRIMARY KEY (mcc,mnc,lac,month)
);
