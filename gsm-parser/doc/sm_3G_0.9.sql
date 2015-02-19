#ifdef SQLITE
#define date_format(x,y) strftime(y,x)
#endif

--  Valid 3G sessions
--  FIXME: Looks like we see not t_release for 3G???
DROP VIEW IF EXISTS valid_si;
CREATE VIEW valid_si AS
SELECT
	session_info.*,
	CASE WHEN(t_call OR (mobile_term AND NOT t_sms)) AND call_presence THEN 1 ELSE 0 END as is_call,
	CASE WHEN t_sms AND sms_presence                                   THEN 1 ELSE 0 END as is_sms,
	CASE WHEN t_locupd AND lu_acc                                      THEN 1 ELSE 0 END as is_lu,
	CASE WHEN cipher > 0                                               THEN 1 ELSE 0 END as is_enc,
	CASE WHEN auth > 0                                                 THEN 1 ELSE 0 END as is_auth,
	CASE WHEN mobile_term AND NOT mobile_orig                          THEN 1 ELSE 0 END as is_mt,
	CASE WHEN mobile_orig AND NOT mobile_term                          THEN 1 ELSE 0 END as is_mo,
	CASE WHEN substr(imsi, 1, 3) = mcc                                 THEN 1 ELSE 0 END as is_homeuser
FROM
	session_info
WHERE
	rat = 1 and domain = 0;

--  Scores per operator
DELETE FROM risk_3G;
INSERT INTO risk_3G
SELECT
	mcc,
	mnc,
	date_format(timestamp, "%Y-%m") as month,
	lac,
	count(*) as total_samples,
	sum(is_call OR is_sms) as intercept_samples,
	sum(is_mo AND (is_call OR is_sms)) as impersonation_samples,
	(sum(is_call+0.0))/count(*) as call_perc,
	(sum(is_sms+0.0))/count(*)  as sms_perc,
	(sum(is_lu+0.0))/count(*)   as lu_perc,
	(sum((is_enc AND (is_call OR is_sms))+0.0))/sum((is_call OR is_sms)+0.0)  as enc_perc,
	(sum((is_enc AND is_lu)+0.0))/sum(is_lu+0.0)  as enc_lu_perc,
	(sum((is_auth AND is_mo)+0.0))/sum(is_mo+0.0) as auth_mo_perc,
	(sum((is_auth AND is_mt)+0.0))/sum(is_mt+0.0) as auth_mt_perc,
	(sum((is_auth AND is_homeuser AND auth = 2)+0.0))/sum((is_auth AND is_homeuser)+0.0) as auth_aka_perc,
	(sum((t_tmsi_realloc AND (is_call OR is_sms))+0.0))/sum((is_call OR is_sms)+0.0) as tmsi_realloc_perc,
	sum(CASE
		WHEN cipher = 0 THEN 0.0
	                    ELSE 1.0 END)/count(*) as intercept3G,
	sum(CASE
		WHEN auth = 2 THEN 1.0	-- UMTS authentication
		WHEN auth = 1 THEN 0.7  -- GSM authentication
				      ELSE 0.0  -- No authentication
		END)/count(*) as impersonation3G
FROM
	valid_si
WHERE
	(is_call OR is_sms OR is_lu)
GROUP BY
	mcc, mnc, month, lac;

-- Clean up invalid operators
DELETE FROM risk_3G
WHERE (mcc, mnc) NOT IN (SELECT DISTINCT mcc, mnc FROM valid_op);

-- Round up scores close to 100%
UPDATE risk_3G set intercept3G = 1.0 where intercept3G > 0.3;
UPDATE risk_3G set enc_perc = 1.0 where enc_perc > 0.3;
