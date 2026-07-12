/* fcc_parse.h — parses FCC ULS EN/HD/AM .dat files (pipe-delimited).
 *
 * Only the fields HamDex actually uses are extracted, by fixed column
 * index, but the full column layouts are documented below for reference
 * (same layout as the FCC's public ULS database documentation).
 *
 * EN_COLS: record_type,unique_system_identifier,uls_file_number,ebf_number,
 *   call_sign,entity_type,licensee_id,entity_name,first_name,mi,last_name,
 *   suffix,phone,fax,email,street_address,city,state,zip_code,po_box,
 *   attention_line,sgin,frn,applicant_type_code,applicant_type_code_other,
 *   status_code,status_date,lic_category_code,linked_license_id,linked_callsign
 *
 * HD_COLS: record_type,unique_system_identifier,uls_file_number,ebf_number,
 *   call_sign,license_status,radio_service_code,grant_date,expired_date,
 *   cancellation_date,eligibility_rule_num,reserved1,alien,alien_government,
 *   alien_corporation,alien_officer,alien_control,revoked,convicted,adjudged,
 *   reserved2,common_carrier,non_common_carrier,private_comm,fixed,mobile,
 *   radiolocation,satellite,developmental_or_sta,interconnected_service,
 *   certifier_first_name,certifier_mi,certifier_last_name,certifier_suffix,
 *   certifier_title,female,black_or_african_american,native_american,
 *   hawaiian,asian,white,hispanic,effective_date,last_action_date,auction_id,
 *   broadcast_services_regulatory_status,band_manager_regulatory_status,
 *   broadcast_services_type_of_radio_service,alien_ruling,licensee_name_change
 *
 * AM_COLS: record_type,unique_system_identifier,uls_file_number,ebf_number,
 *   call_sign,operator_class,group_code,region_code,trustee_call_sign,
 *   trustee_indicator,physician_certification,ve_signature,
 *   systematic_call_sign_change,vanity_call_sign_change,vanity_relationship,
 *   previous_call_sign,previous_operator_class,trustee_name
 */
#ifndef HAMDEX_FCC_PARSE_H
#define HAMDEX_FCC_PARSE_H

#include <glib.h>

typedef struct {
    char *call_sign;
    char *entity_type;
    char *entity_name;
    char *first_name;
    char *last_name;
    char *street_address;
    char *city;
    char *state;
    char *zip_code;
    char *frn;
    char *applicant_type_code;
} FccEnRow;

typedef struct {
    char *call_sign;
    char *license_status;
    char *radio_service_code;
    char *grant_date;
    char *expired_date;
    char *cancellation_date;
    char *effective_date;
} FccHdRow;

typedef struct {
    char *call_sign;
    char *operator_class;
    char *group_code;
    char *trustee_call_sign;
    char *trustee_name;
    char *previous_call_sign;
} FccAmRow;

/* Parses `text` (the content of an .dat file) and returns a GHashTable
 * mapping unique_system_identifier (char*) -> row struct (owned).
 * Caller frees with fcc_en_table_free / fcc_hd_table_free / fcc_am_table_free. */
GHashTable *fcc_parse_en(const char *text);
GHashTable *fcc_parse_hd(const char *text);
GHashTable *fcc_parse_am(const char *text);

void fcc_en_table_free(GHashTable *t);
void fcc_hd_table_free(GHashTable *t);
void fcc_am_table_free(GHashTable *t);

#endif
