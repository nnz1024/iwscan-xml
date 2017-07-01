/* Based on
 *      Wireless Tools
 * by
 *              Jean II - HPLB 97->99 - HPL 99->09
 *
 * Common subroutines to all the wireless tools...
 *
 * This file is released under the GPL license.
 *     Copyright (c) 1997-2009 Jean Tourrilhes <jt@hpl.hp.com>
 *     Disfigured in 2016 by Sergey Ptashnick <0comffdiz@inbox.ru>
 */

#include "iwscan.h"

#include <time.h>

#define FAILMSG "<Interface name='%s' failed='1' error='%s' stamp='%s' />\n"

typedef int (*iw_enum_handler_with_fd)(FILE *   fd,
                                       int      skfd,
                                       char *   ifname,
                                       char *   args[],
                                       int      count);

/*** From iwlib-private.h ***/

/* Paths */
#define PROC_NET_DEV            "/proc/net/dev"

/* Some useful constants */
#define KILO    1e3
#define MEGA    1e6
#define GIGA    1e9

struct iw_pk_event
{
	__u16           len;                    /* Real lenght of this stuff */
	__u16           cmd;                    /* Wireless IOCTL */
	union iwreq_data        u;              /* IOCTL fixed payload */
}	__attribute__ ((packed));

struct  iw_pk_point
{
	void __user   *pointer;       /* Pointer to the data  (in user space) */
	__u16         length;         /* number of fields or size in bytes */
	__u16         flags;          /* Optional params */
}	__attribute__ ((packed));

#define IW_EV_LCP_PK2_LEN       (sizeof(struct iw_pk_event) - sizeof(union iwreq_data))
#define IW_EV_POINT_PK2_LEN     (IW_EV_LCP_PK2_LEN + sizeof(struct iw_pk_point) - IW_EV_POINT_OFF)

static inline char *
iw_saether_ntop(const struct sockaddr *sap, char* bufp)
{
	iw_ether_ntop((const struct ether_addr *) sap->sa_data, bufp);
	return bufp;
}
/*
 * Input an Ethernet Socket Address and convert to binary.
  */
static inline int
iw_saether_aton(const char *bufp, struct sockaddr *sap)
{
	sap->sa_family = ARPHRD_ETHER;
	return iw_ether_aton(bufp, (struct ether_addr *) sap->sa_data);
}

/*** From iwlist.c ***/
/****************************** TYPES ******************************/
/*
 * Scan state and meta-information, used to decode events...
 */
typedef struct iwscan_state
{
  /* State */
  int                   ap_num;         /* Access Point number 1->N */
  int                   val_index;      /* Value in table 0->(N-1) */
} iwscan_state;

typedef struct iwscan_state_ext
{
  /* State */
  int                   ap_num;         /* Access Point number 1->N */
  int                   val_index;      /* Value in table 0->(N-1) */
  int                   extra_ie;       /* We print array of Extras and IE */
} iwscan_state_ext;

/*
 * Bit to name mapping
 */
typedef struct iwmask_name
{
  unsigned int  mask;   /* bit mask for the value */
  const char *  name;   /* human readable name for the value */
} iwmask_name;

/*
 * Types of authentication parameters
 */
typedef struct iw_auth_descr
{
  int                           value;          /* Type of auth value */
  const char *                  label;          /* User readable version */
  const struct iwmask_name *    names;          /* Names for this value */
  const int                     num_names;      /* Number of names */
} iw_auth_descr;

/**************************** CONSTANTS ****************************/

#define IW_SCAN_HACK            0x8000

#define IW_EXTKEY_SIZE  (sizeof(struct iw_encode_ext) + IW_ENCODING_TOKEN_MAX)

/* ------------------------ WPA CAPA NAMES ------------------------ */
/*
 * This is the user readable name of a bunch of WPA constants in wireless.h
 * Maybe this should go in iwlib.c ?
 */

#define IW_ARRAY_LEN(x) (sizeof(x)/sizeof((x)[0]))

//static const struct iwmask_name iw_enc_mode_name[] = {
//  { IW_ENCODE_RESTRICTED,     "restricted" },
//  { IW_ENCODE_OPEN,           "open" },
//};
//#define       IW_ENC_MODE_NUM         IW_ARRAY_LEN(iw_enc_mode_name)

static const struct iwmask_name iw_auth_capa_name[] = {
  { IW_ENC_CAPA_WPA,            "WPA" },
  { IW_ENC_CAPA_WPA2,           "WPA2" },
  { IW_ENC_CAPA_CIPHER_TKIP,    "CIPHER-TKIP" },
  { IW_ENC_CAPA_CIPHER_CCMP,    "CIPHER-CCMP" },
};
#define IW_AUTH_CAPA_NUM        IW_ARRAY_LEN(iw_auth_capa_name)

static const struct iwmask_name iw_auth_cypher_name[] = {
  { IW_AUTH_CIPHER_NONE,        "none" },
  { IW_AUTH_CIPHER_WEP40,       "WEP-40" },
  { IW_AUTH_CIPHER_TKIP,        "TKIP" },
  { IW_AUTH_CIPHER_CCMP,        "CCMP" },
  { IW_AUTH_CIPHER_WEP104,      "WEP-104" },
};
#define IW_AUTH_CYPHER_NUM      IW_ARRAY_LEN(iw_auth_cypher_name)

static const struct iwmask_name iw_wpa_ver_name[] = {
  { IW_AUTH_WPA_VERSION_DISABLED,       "disabled" },
  { IW_AUTH_WPA_VERSION_WPA,            "WPA" },
  { IW_AUTH_WPA_VERSION_WPA2,           "WPA2" },
};
#define IW_WPA_VER_NUM          IW_ARRAY_LEN(iw_wpa_ver_name)

static const struct iwmask_name iw_auth_key_mgmt_name[] = {
  { IW_AUTH_KEY_MGMT_802_1X,    "802.1x" },
  { IW_AUTH_KEY_MGMT_PSK,       "PSK" },
};
#define IW_AUTH_KEY_MGMT_NUM    IW_ARRAY_LEN(iw_auth_key_mgmt_name)

static const struct iwmask_name iw_auth_alg_name[] = {
  { IW_AUTH_ALG_OPEN_SYSTEM,    "open" },
  { IW_AUTH_ALG_SHARED_KEY,     "shared-key" },
  { IW_AUTH_ALG_LEAP,           "LEAP" },
};
#define IW_AUTH_ALG_NUM         IW_ARRAY_LEN(iw_auth_alg_name)

static const struct iw_auth_descr       iw_auth_settings[] = {
  { IW_AUTH_WPA_VERSION, "WPA version", iw_wpa_ver_name, IW_WPA_VER_NUM },
  { IW_AUTH_KEY_MGMT, "Key management", iw_auth_key_mgmt_name, IW_AUTH_KEY_MGMT_NUM },
  { IW_AUTH_CIPHER_PAIRWISE, "Pairwise cipher", iw_auth_cypher_name, IW_AUTH_CYPHER_NUM },
  { IW_AUTH_CIPHER_GROUP, "Pairwise cipher", iw_auth_cypher_name, IW_AUTH_CYPHER_NUM },
  { IW_AUTH_TKIP_COUNTERMEASURES, "TKIP countermeasures", NULL, 0 },
  { IW_AUTH_DROP_UNENCRYPTED, "Drop unencrypted", NULL, 0 },
  { IW_AUTH_80211_AUTH_ALG, "Authentication algorithm", iw_auth_alg_name, IW_AUTH_ALG_NUM },
  { IW_AUTH_RX_UNENCRYPTED_EAPOL, "Receive unencrypted EAPOL", NULL, 0 },
  { IW_AUTH_ROAMING_CONTROL, "Roaming control", NULL, 0 },
  { IW_AUTH_PRIVACY_INVOKED, "Privacy invoked", NULL, 0 },
};
#define IW_AUTH_SETTINGS_NUM            IW_ARRAY_LEN(iw_auth_settings)

/* Values for the IW_ENCODE_ALG_* returned by SIOCSIWENCODEEXT */
/*static const char *     iw_encode_alg_name[] = {
        "none",
        "WEP",
        "TKIP",
        "CCMP",
        "unknown"
};*/
#define IW_ENCODE_ALG_NUM               IW_ARRAY_LEN(iw_encode_alg_name)

#ifndef IW_IE_CIPHER_NONE
/* Cypher values in GENIE (pairwise and group) */
#define IW_IE_CIPHER_NONE       0
#define IW_IE_CIPHER_WEP40      1
#define IW_IE_CIPHER_TKIP       2
#define IW_IE_CIPHER_WRAP       3
#define IW_IE_CIPHER_CCMP       4
#define IW_IE_CIPHER_WEP104     5
/* Key management in GENIE */
#define IW_IE_KEY_MGMT_NONE     0
#define IW_IE_KEY_MGMT_802_1X   1
#define IW_IE_KEY_MGMT_PSK      2
#endif  /* IW_IE_CIPHER_NONE */

/* Values for the IW_IE_CIPHER_* in GENIE */
static const char *     iw_ie_cypher_name[] = {
        "none",
        "WEP-40",
        "TKIP",
        "WRAP",
        "CCMP",
        "WEP-104",
};
#define IW_IE_CYPHER_NUM        IW_ARRAY_LEN(iw_ie_cypher_name)

/* Values for the IW_IE_KEY_MGMT_* in GENIE */
static const char *     iw_ie_key_mgmt_name[] = {
        "none",
        "802.1x",
        "PSK",
};
#define IW_IE_KEY_MGMT_NUM      IW_ARRAY_LEN(iw_ie_key_mgmt_name)

/*********************** BITRATE SUBROUTINES ***********************/

/*------------------------------------------------------------------*/
/*
 * Output a bitrate with proper scaling
 */
void
iw_print_bitrate_xml(char * buffer,
                 int    buflen,
                 int    bitrate)
{
  double        rate = bitrate;
  char          scale;
  int           divisor;

  if(rate >= GIGA)
    {
      scale = 'G';
      divisor = GIGA;
    }
  else
    {
      if(rate >= MEGA)
        {
          scale = 'M';
          divisor = MEGA;
        }
      else
        {
          scale = 'k';
          divisor = KILO;
        }
    }
  snprintf(buffer, buflen, "<BitRate unit='%cb/s'>%g</BitRate>", 
           scale, rate / divisor);
}

/*------------------------------------------------------------------*/
/*
 * Escape non-ASCII characters from ESSID.
 * This allow us to display those weirds characters to the user.
 *
 * Source is 32 bytes max.
 * Destination buffer needs to be at least 129 bytes, will be NUL
 * terminated.
 */
void
iw_essid_escape_xml(char *          dest,
                const char *    src,
                const int       slen)
{
  const unsigned char * s = (const unsigned char *) src;
  const unsigned char * e = s + slen;
  char *                d = dest;

  /* Look every character of the string */
  while(s < e)
    {
      int       isescape;

      /* Escape the escape to avoid ambiguity.
       * We do a fast path test for performance reason. Compiler will
       * optimise all that ;-) */
      if(*s == '\\')
        {
          /* Check if we would confuse it with an escape sequence */
          if((e-s) > 4 && (s[1] == 'x')
             && (isxdigit(s[2])) && (isxdigit(s[3])))
            {
              isescape = 1;
            }
          else
            isescape = 0;
        }
      else
        isescape = 0;
      

      /* Is it a non-ASCII character ??? */
      if(isescape || !isascii(*s) || iscntrl(*s))
        {
          /* Escape */
          sprintf(d, "\\x%02X", *s);
          d += 4;
        }
      /* Some XML-specific escapes */
      else if (*s == '<')
        {
          sprintf(d, "&lt;");
          d += 4;
        }
      else if (*s == '>')
        {
          sprintf(d, "&gt;");
          d += 4;
        }
      else if (*s == '&')
        {
          sprintf(d, "&amp;");
          d += 5;
        }
      else
        {
          /* Plain ASCII, just copy */
          *d = *s;
          d++;
        }
      s++;
    }

  /* NUL terminate destination */
  *d = '\0';
}

/************************* WPA SUBROUTINES *************************/

/*------------------------------------------------------------------*/
/*
 * Print the name corresponding to a value, with overflow check.
 */
static void
iw_print_value_name(FILE *			fd,
		    unsigned int                value,
                    const char *                names[],
                    const unsigned int          num_names)
{
  if(value >= num_names)
    fprintf(fd, " value='%d'>Unknown", value);
  else
    fprintf(fd, " value='%d'>%s", value, names[value]);
}

/*------------------------------------------------------------------*/
/*
 * Parse, and display the results of an unknown IE.
 *
 */
static void
iw_print_ie_unknown_xml(FILE *			fd,
			unsigned char * 	iebuf,
			int			buflen)
{
  int   ielen = iebuf[1] + 2;
  int   i;

  if(ielen > buflen)
    ielen = buflen;

  fprintf(fd, "\n  <IE>");
  for(i = 0; i < ielen; i++)
    fprintf(fd, "%02X", iebuf[i]);
  fprintf(fd, "</IE>");
}

/*------------------------------------------------------------------*/
/*
 * Parse, and display the results of a WPA or WPA2 IE.
 *
 */
static inline void
iw_print_ie_wpa_xml(FILE *	fd,
		unsigned char * iebuf,
		int             buflen)
{
  int                   ielen = iebuf[1] + 2;
  int                   offset = 2;     /* Skip the IE id, and the length. */
  unsigned char         wpa1_oui[3] = {0x00, 0x50, 0xf2};
  unsigned char         wpa2_oui[3] = {0x00, 0x0f, 0xac};
  unsigned char *       wpa_oui;
  int                   i;
  uint16_t              ver = 0;
  uint16_t              cnt = 0;

  if(ielen > buflen)
    ielen = buflen;

#ifdef DEBUG
  /* Debugging code. In theory useless, because it's debugged ;-) */
  printf("IE raw value %d [%02X", buflen, iebuf[0]);
  for(i = 1; i < buflen; i++)
    printf(":%02X", iebuf[i]);
  printf("]\n");
#endif

  switch(iebuf[0])
    {
    case 0x30:          /* WPA2 */
      /* Check if we have enough data */
      if(ielen < 4)
        {
          iw_print_ie_unknown_xml(fd, iebuf, buflen);
          return;
        }

      wpa_oui = wpa2_oui;
      break;

    case 0xdd:          /* WPA or else */
      wpa_oui = wpa1_oui;

      /* Not all IEs that start with 0xdd are WPA. 
       * So check that the OUI is valid. Note : offset==2 */
      if((ielen < 8)
         || (memcmp(&iebuf[offset], wpa_oui, 3) != 0)
         || (iebuf[offset + 3] != 0x01))
        {
          iw_print_ie_unknown_xml(fd, iebuf, buflen);
          return;
        }

      /* Skip the OUI type */
      offset += 4;
      break;

    default:
      return;
    }

  /* Pick version number (little endian) */
  ver = iebuf[offset] | (iebuf[offset + 1] << 8);
  offset += 2;
  if(iebuf[0] == 0xdd)
    fprintf(fd, "\n  <WPA type='1' version='%d'>", ver);
  else if(iebuf[0] == 0x30)
    fprintf(fd, "\n  <WPA type='2' version='%d'>", ver);
  else
    fprintf(fd, "\n  <WPA type='0'>");

  /* From here, everything is technically optional. */

  /* Check if we are done */
  if(ielen < (offset + 4))
    {
      /* We have a short IE.  So we should assume TKIP/TKIP. */
      fprintf(fd, "\n    <GroupCiphers count='1'>"
                "\n      <Cipher>TKIP</Cipher>\n    </GroupCiphers>");
      fprintf(fd, "\n    <PairwiseCiphers count='1'>"
                "\n      <Cipher>TKIP</Cipher>\n    </PairwiseCiphers>");
      return;
    }

  /* Next we have our group cipher. */
  if(memcmp(&iebuf[offset], wpa_oui, 3) != 0)
    {
      fprintf(fd, "\n    <GroupCiphers count='1'>"
                "\n      <Cipher>Proprietary</Cipher>\n    </GroupCiphers>");
    }
  else
    {
      fprintf(fd, "\n    <GroupCiphers count='1'>\n      <Cipher");
      iw_print_value_name(fd, iebuf[offset+3],
                          iw_ie_cypher_name, IW_IE_CYPHER_NUM);
      fprintf(fd, "</Cipher>\n    </GroupCiphers>");
    }
  offset += 4;

  /* Check if we are done */
  if(ielen < (offset + 2))
    {
      /* We don't have a pairwise cipher, or auth method. Assume TKIP. */
      fprintf(fd, "\n    <PairwiseCiphers count='1'>"
                "\n      <Cipher>TKIP</Cipher>\n    </PairwiseCiphers>");
      return;
    }

  /* Otherwise, we have some number of pairwise ciphers. */
  cnt = iebuf[offset] | (iebuf[offset + 1] << 8);
  offset += 2;
  fprintf(fd, "\n    <PairwiseCiphers count='%d'>", cnt);

  if(ielen < (offset + 4*cnt))
    {
        /* OOPS! Leaving now */
        fprintf(fd, "\n    </PairwiseCiphers>\n  </WPA>");
        return;
    }

  for(i = 0; i < cnt; i++)
    {
      fprintf(fd, "\n      <Cipher");
      if(memcmp(&iebuf[offset], wpa_oui, 3) != 0)
        {
          fprintf(fd, ">Proprietary");
        }
      else
        {
          iw_print_value_name(fd, iebuf[offset+3],
                              iw_ie_cypher_name, IW_IE_CYPHER_NUM);
        }
      offset+=4;
      fprintf(fd, "</Cipher>");
    }
  fprintf(fd, "\n    </PairwiseCiphers>");

  /* Check if we are done */
  if(ielen < (offset + 2))
    {
        fprintf(fd, "\n  </WPA>");
        return;
    }

  /* Now, we have authentication suites. */
  cnt = iebuf[offset] | (iebuf[offset + 1] << 8);
  offset += 2;
  fprintf(fd, "\n    <AuthenticationSuites count='%d'>", cnt);

  if(ielen < (offset + 4*cnt))
    {
        /* OOPS! Leaving now */
        fprintf(fd, "</AuthenticationSuites>\n  </WPA>");
        return;
    }

  for(i = 0; i < cnt; i++)
    {
      fprintf(fd, "\n      <AuthenticationSuite");
      if(memcmp(&iebuf[offset], wpa_oui, 3) != 0)
        {
          fprintf(fd, ">Proprietary");
        }
      else
        {
          iw_print_value_name(fd, iebuf[offset+3],
                              iw_ie_key_mgmt_name, IW_IE_KEY_MGMT_NUM);
        }
       fprintf(fd, "</AuthenticationSuite>");
       offset+=4;
     }
  fprintf(fd, "\n    </AuthenticationSuites>");

  /* Check if we are done */
  if(ielen < (offset + 1))
    {
        fprintf(fd, "\n  </WPA>");
        return;
    }

  /* Otherwise, we have capabilities bytes.
   * For now, we only care about preauth which is in bit position 1 of the
   * first byte.  (But, preauth with WPA version 1 isn't supposed to be 
   * allowed.) 8-) */
  if(iebuf[offset] & 0x01)
      fprintf(fd, "\n    <Preauthentication supported='1' />");
  else
      fprintf(fd, "\n    <Preauthentication supported='0' />");
    
  fprintf(fd, "\n  </WPA>");
}

/*------------------------------------------------------------------*/
/*
 * Process a generic IE and display the info in human readable form
 * for some of the most interesting ones.
 * For now, we only decode the WPA IEs.
 */
static inline void
iw_print_gen_ie_xml(FILE *	fd,
		unsigned char * buffer,
                int             buflen)
{
  int offset = 0;

  /* Loop on each IE, each IE is minimum 2 bytes */
  while(offset <= (buflen - 2))
    {
      /* Check IE type */
      switch(buffer[offset])
        {
        case 0xdd:      /* WPA1 (and other) */
        case 0x30:      /* WPA2 */
          iw_print_ie_wpa_xml(fd, buffer + offset, buflen);
          break;
        default:
          iw_print_ie_unknown_xml(fd, buffer + offset, buflen);
        }
      /* Skip over this IE to the next one in the list. */
      offset += buffer[offset+1] + 2;
    }
}

void
iw_print_freq_xml(char *    buffer,
              int       buflen,
              double    freq,
              int       channel,
              int       freq_flags)
{
  /* Check if channel only */
  if(freq < KILO)      
     snprintf(buffer, buflen, "\n  <Channel>%g</Channel>", freq);
  else
    {
      /*if (freq_flags & IW_FREQ_FIXED)
          snprintf(buffer, buflen, "\n  \"FixedFreq\": 1");
      else
          snprintf(buffer, buflen, "\n  \"FixedFreq\": 0");*/
      /* Frequency. Check if we have a channel as well */
      if(channel >= 0)
        snprintf(buffer, buflen, "\n  <Frequency fixed='%d' unit='GHz'>%g"
                "</Frequency>\n  <Channel>%d</Channel>",
                (freq_flags & IW_FREQ_FIXED) > 0, freq*1e-9, channel);
      else
        snprintf(buffer, buflen, "\n  <Frequency unit='GHz'>%g</Frequency>", 
                 freq*1e-9);
    }
}

void
iw_print_stats_xml(char *           buffer,
               int              buflen,
               const iwqual *   qual,
               const iwrange *  range,
               int              has_range)
{
  int           len;

  /* People are very often confused by the 8 bit arithmetic happening
   * here.
   * All the values here are encoded in a 8 bit integer. 8 bit integers
   * are either unsigned [0 ; 255], signed [-128 ; +127] or
   * negative [-255 ; 0].
   * Further, on 8 bits, 0x100 == 256 == 0.
   *
   * Relative/percent values are always encoded unsigned, between 0 and 255.
   * Absolute/dBm values are always encoded between -192 and 63.
   * (Note that up to version 28 of Wireless Tools, dBm used to be
   *  encoded always negative, between -256 and -1).
   *
   * How do we separate relative from absolute values ?
   * The old way is to use the range to do that. As of WE-19, we have
   * an explicit IW_QUAL_DBM flag in updated...
   * The range allow to specify the real min/max of the value. As the
   * range struct only specify one bound of the value, we assume that
   * the other bound is 0 (zero).
   * For relative values, range is [0 ; range->max].
   * For absolute values, range is [range->max ; 63].
   *
   * Let's take two example :
   * 1) value is 75%. qual->value = 75 ; range->max_qual.value = 100
   * 2) value is -54dBm. noise floor of the radio is -104dBm.
   *    qual->value = -54 = 202 ; range->max_qual.value = -104 = 152
   *
   * Jean II
   */

  /* Just do it...
   * The old way to detect dBm require both the range and a non-null
   * level (which confuse the test). The new way can deal with level of 0
   * because it does an explicit test on the flag. */
  if(has_range && ((qual->level != 0)
                   || (qual->updated & (IW_QUAL_DBM | IW_QUAL_RCPI))))
    {
      /* Deal with quality : always a relative value */
      if(!(qual->updated & IW_QUAL_QUAL_INVALID))
        {
          /*len = snprintf(buffer, buflen, "Quality%c%d/%d  ",
                         qual->updated & IW_QUAL_QUAL_UPDATED ? '=' : ':',
                         qual->qual, range->max_qual.qual);*/
          len = snprintf(buffer, buflen, "\n  <Quality updated='%d' max='%d'>" 
                        "%d</Quality>", 
                         ((qual->updated & IW_QUAL_QUAL_UPDATED) > 0),
                         range->max_qual.qual, qual->qual);
          buffer += len;
          buflen -= len;
        }

      /* Check if the statistics are in RCPI (IEEE 802.11k) */
      if(qual->updated & IW_QUAL_RCPI)
        {
          /* Deal with signal level in RCPI */
          /* RCPI = int{(Power in dBm +110)*2} for 0dbm > Power > -110dBm */
          if(!(qual->updated & IW_QUAL_LEVEL_INVALID))
            {
              double    rcpilevel = (qual->level / 2.0) - 110.0;
              len = snprintf(buffer, buflen, "\n  <SignalLevel updated='%d'>"
                            "%g</SignalLevel>",
                            ((qual->updated & IW_QUAL_LEVEL_UPDATED) > 0),
                            rcpilevel);
              buffer += len;
              buflen -= len;
            }

          /* Deal with noise level in dBm (absolute power measurement) */
          if(!(qual->updated & IW_QUAL_NOISE_INVALID))
            {
              double    rcpinoise = (qual->noise / 2.0) - 110.0;
              len = snprintf(buffer, buflen, "\n  <NoiseLevel updated='%d'>"
                             "%g</NoiseLevel>",
                             ((qual->updated & IW_QUAL_NOISE_UPDATED) > 0),
                             rcpinoise);
            }
        }
      else
        {
          /* Check if the statistics are in dBm */
          if((qual->updated & IW_QUAL_DBM)
             || (qual->level > range->max_qual.level))
            {
              /* Deal with signal level in dBm  (absolute power measurement) */
              if(!(qual->updated & IW_QUAL_LEVEL_INVALID))
                {
                  int   dblevel = qual->level;
                  /* Implement a range for dBm [-192; 63] */
                  if(qual->level >= 64)
                    dblevel -= 0x100;
                  len = snprintf(buffer, buflen, 
                                 "\n  <SignalLevel updated='%d'>"
                                 "%d</SignalLevel>",
                                 ((qual->updated & IW_QUAL_LEVEL_UPDATED) > 0),
                                 dblevel);
                  buffer += len;
                  buflen -= len;
                }

              /* Deal with noise level in dBm (absolute power measurement) */
              if(!(qual->updated & IW_QUAL_NOISE_INVALID))
                {
                  int   dbnoise = qual->noise;
                  /* Implement a range for dBm [-192; 63] */
                  if(qual->noise >= 64)
                    dbnoise -= 0x100;
                  len = snprintf(buffer, buflen, 
                                 "\n  <NoiseLevel updated='%d'>"
                                 "%d</NoiseLevel>",
                                 ((qual->updated & IW_QUAL_NOISE_UPDATED) > 0),
                                 dbnoise);
                }
            }
          else
            {
              /* Deal with signal level as relative value (0 -> max) */
              if(!(qual->updated & IW_QUAL_LEVEL_INVALID))
                {
                  len = snprintf(buffer, buflen, 
                                 "\n  <SignalLevelRelative updated='%d'"
                                 " max='%d'>%d</SignalLevelRelative>",
                                 ((qual->updated & IW_QUAL_LEVEL_UPDATED) > 0),
                                 range->max_qual.level, qual->level);
                  buffer += len;
                  buflen -= len;
                }

              /* Deal with noise level as relative value (0 -> max) */
              if(!(qual->updated & IW_QUAL_NOISE_INVALID))
                {
                  len = snprintf(buffer, buflen,
                                 "\n  <NoiseLevelRelative updated='%d'"
                                 " max='%d'>%d</NoiseLevelRelative>",
                                 ((qual->updated & IW_QUAL_NOISE_UPDATED) > 0),
                                 range->max_qual.noise, qual->noise);
                }
            }
        }
    }
  else
    {
      /* We can't read the range, so we don't know... */
      snprintf(buffer, buflen,
               "\n  <QualityParrots>%d</QualityParrots>"
               "\n  <SignalLevelParrots>%d</SignalLevelParrots>"
               "\n  <NoiseLevelParrots>%d</NoiseLevelParrots>",
               qual->qual, qual->level, qual->noise);
    }
}


/***************************** SCANNING *****************************/
/*
 * This one behave quite differently from the others
 *
 * Note that we don't use the scanning capability of iwlib (functions
 * iw_process_scan() and iw_scan()). The main reason is that
 * iw_process_scan() return only a subset of the scan data to the caller,
 * for example custom elements and bitrates are ommited. Here, we
 * do the complete job...
 */

/*------------------------------------------------------------------*/
/*
 * Print one element from the scanning results
 */
static inline void
print_scanning_token_xml(FILE *			fd,
		     struct stream_descr *      stream, /* Stream of events */
                     struct iw_event *          event,  /* Extracted token */
                     struct iwscan_state_ext *      state,
                     struct iw_range *  iw_range,       /* Range info */
                     int                has_range)
{
  char          buffer[128];    /* Temporary buffer */

  /* Now, let's decode the event */
  switch(event->cmd)
    {
    case SIOCGIWAP:
      /*if (state->extra_ie) {
          printf("]\n");
          state->extra_ie = 0;
      }*/
      if (state->ap_num > 1) {
          fprintf(fd, "\n</AP>\n");
      }
      fprintf(fd, "<AP cell='%d'>\n  <Address>%s</Address>", state->ap_num,
             iw_saether_ntop(&event->u.ap_addr, buffer));
      state->ap_num++;
      break;
    case SIOCGIWNWID:
      if(event->u.nwid.disabled)
        fprintf(fd, "\n  <NWID enabled='0'/>");
      else
        fprintf(fd, "\n  <NWID enabled='1'>%X</NWID>", event->u.nwid.value);
      break;
    case SIOCGIWFREQ:
      {
        double          freq;                   /* Frequency/channel */
        int             channel = -1;           /* Converted to channel */
        freq = iw_freq2float(&(event->u.freq));
        /* Convert to channel if possible */
        if(has_range)
          channel = iw_freq_to_channel(freq, iw_range);
        iw_print_freq_xml(buffer, sizeof(buffer),
                      freq, channel, event->u.freq.flags);
        fprintf(fd, "%s", buffer);
      }
      break;
    case SIOCGIWMODE:
      /* Note : event->u.mode is unsigned, no need to check <= 0 */
      if(event->u.mode >= IW_NUM_OPER_MODE)
        event->u.mode = IW_NUM_OPER_MODE;
      fprintf(fd, "\n  <Mode>%s</Mode>", iw_operation_mode[event->u.mode]);
      break;
    case SIOCGIWNAME:
      fprintf(fd, "\n  <Protocol>%s</Protocol>", event->u.name);
      break;
    case SIOCGIWESSID:
      {
        char essid[5*IW_ESSID_MAX_SIZE+1]; /* &amp; have five chars */
        memset(essid, '\0', sizeof(essid));
        if((event->u.essid.pointer) && (event->u.essid.length))
          iw_essid_escape_xml(essid,
                          event->u.essid.pointer, event->u.essid.length);
        if(event->u.essid.flags)
          {
            /* Does it have an ESSID index ? */
            if((event->u.essid.flags & IW_ENCODE_INDEX) > 1)
              fprintf(fd, "\n  <ESSID flags='%d'>%s</ESSID>",
                     (event->u.essid.flags & IW_ENCODE_INDEX), essid);
            else
              fprintf(fd, "\n  <ESSID>%s</ESSID>", essid);
          }
        else
          fprintf(fd, "\n  <ESSID />");
      }
      break;
    case SIOCGIWENCODE:
      {
        unsigned char   key[IW_ENCODING_TOKEN_MAX], key_enabled;
        if(event->u.data.pointer)
          memcpy(key, event->u.data.pointer, event->u.data.length);
        else
          event->u.data.flags |= IW_ENCODE_NOKEY;
        key_enabled = ((event->u.data.flags & IW_ENCODE_DISABLED) > 0);
        fprintf(fd, "\n  <EncryptionKey enabled='%d'", key_enabled);
        if (!key_enabled)
          fprintf(fd, " />");
        else
          {
            /* Display the key */
            iw_print_key(buffer, sizeof(buffer), key, event->u.data.length,
                         event->u.data.flags);
            fprintf(fd, ">\n    <Key>%s</Key>", buffer);

            /* Other info... */
            if((event->u.data.flags & IW_ENCODE_INDEX) > 1)
              fprintf(fd, "\n    <EncodeIndex>%d</EncodeIndex>", 
                     event->u.data.flags & IW_ENCODE_INDEX);
            if(event->u.data.flags & IW_ENCODE_RESTRICTED)
              fprintf(fd, "\n    <SecurityMode>restricted</SecurityMode>");
            if(event->u.data.flags & IW_ENCODE_OPEN)
              fprintf(fd, "\n    <SecurityMode>open</SecurityMode>");
            fprintf(fd, "\n  </EncryptionKey>");
          }
      }
      break;
    case SIOCGIWRATE:
      if(state->val_index == 0)
        fprintf(fd, "\n  <BitRates>");
      /*else
        if((state->val_index % 5) == 0)
          printf("\n                              ");
        else
        printf(", ");*/
      iw_print_bitrate_xml(buffer, sizeof(buffer), event->u.bitrate.value);
      fprintf(fd, "\n    %s", buffer);
      /* Check for termination */
      if(stream->value == NULL)
        {
          fprintf(fd, "\n  </BitRates>");
          state->val_index = 0;
        }
      else
        state->val_index++;
      break;
     case SIOCGIWMODUL:
      {
        unsigned int    modul = event->u.param.value;
        int             i;
        /*int             n = 0;*/
        fprintf(fd, "\n  <Modulations>");
        for(i = 0; i < IW_SIZE_MODUL_LIST; i++)
          {
            if((modul & iw_modul_list[i].mask) == iw_modul_list[i].mask)
              {
                /*if((n++ % 8) == 7)
                  printf("\n                        ");
                else*/
                  fprintf(fd, ",");
                fprintf(fd, "\n    <Modulation>%s</Modulation>", iw_modul_list[i].cmd);
              }
          }
        fprintf(fd, "\n  </Modulations>");
      }
      break;
    case IWEVQUAL:
      iw_print_stats_xml(buffer, sizeof(buffer),
                     &event->u.qual, iw_range, has_range);
      fprintf(fd, "%s", buffer);
      break;
    case IWEVGENIE:
      /* Informations Elements are complex, let's do only some of them */
      /*if (!state->extra_ie) {
          printf(",\n  \"ExtraAndIE\": [\"\"");
          state->extra_ie = 1;
      }*/
      iw_print_gen_ie_xml(fd, event->u.data.pointer, event->u.data.length);
      break;
    case IWEVCUSTOM:
      {
        /*if (!state->extra_ie) {
            printf(",\n  \"ExtraAndIE\": [\"\"");
            state->extra_ie = 1;
        }*/
        char custom[IW_CUSTOM_MAX+1];
        if((event->u.data.pointer) && (event->u.data.length))
          memcpy(custom, event->u.data.pointer, event->u.data.length);
        custom[event->u.data.length] = '\0';
        fprintf(fd, "\n  <Extra>%s</Extra>", custom);
      }
      break;
    default:
      /*if (!state->extra_ie) {
          printf(",\n  \"ExtraAndIE\": [\"\"");
          state->extra_ie = 1;
      }*/
      fprintf(fd, "\n  <UnknownCommand>0x%04X</UnknownCommand>", event->cmd);
   }    /* switch(event->cmd) */
}

/* Stolen "as is" from iwlib.c for using in iw_enum_devices_xml below */
static inline char *
iw_get_ifname(char *    name,   /* Where to store the name */
              int       nsize,  /* Size of name buffer */
              char *    buf)    /* Current position in buffer */
{
  char *        end;

  /* Skip leading spaces */
  while(isspace(*buf))
    buf++;

  /* Get name up to the last ':'. Aliases may contain ':' in them,
   * but the last one should be the separator */
  end = strrchr(buf, ':');

  /* Not found ??? To big ??? */
  if((end == NULL) || (((end - buf) + 1) > nsize))
    return(NULL);

  /* Copy */
  memcpy(name, buf, (end - buf));
  name[end - buf] = '\0';

  /* Return value currently unused, just make sure it's non-NULL */
  return(end);
}

void iws_format_timestamp (char * stamp_buf)
  {
    time_t      stamp;
    struct tm * stamp_ti;

    time(&stamp);
    stamp_ti = gmtime(&stamp);
    strftime(stamp_buf, IWS_TIMESTAMP_LENGTH, IWS_TIMESTAMP_FORMAT, stamp_ti);
}


void
iw_enum_devices_xml(FILE *              fd,
                int                     skfd,
                iw_enum_handler_with_fd fn,
                char *                  args[],
                int                     count)
{
  char          buff[1024];
  FILE *        fh;
  struct ifconf ifc;
  struct ifreq *ifr;
  int           i;
  char 	        stamp_buf[IWS_TIMESTAMP_LENGTH];

  /* Set timestamp */
  iws_format_timestamp((char *) &stamp_buf);

  /* Print (a part of) opening tag */
  fprintf(fd, "<Scan stamp='%s'", (char *) &stamp_buf);

  /* Check if /proc/net/dev is available */
  fh = fopen(PROC_NET_DEV, "r");

  if(fh != NULL)
    {
      /* Success : use data from /proc/net/wireless */

      /* Let's finish the tag */
      fprintf(fd, ">\n");

      /* Eat 2 lines of header */
      fgets(buff, sizeof(buff), fh);
      fgets(buff, sizeof(buff), fh);

      /* Read each device line */
      while(fgets(buff, sizeof(buff), fh))
        {
          char  name[IFNAMSIZ + 1];
          char *s;

          /* Skip empty or almost empty lines. It seems that in some
           * cases fgets return a line with only a newline. */
          if((buff[0] == '\0') || (buff[1] == '\0'))
            continue;

          /* Extract interface name */
          s = iw_get_ifname(name, sizeof(name), buff);

          if(!s)
            {
              /* Failed to parse, complain and continue */
              fprintf(fd, FAILMSG, IWSCAN_ALL_IFACES, "Cannot parse " 
                  PROC_NET_DEV, (char *) &stamp_buf);
            }
          else if (strcmp(name, IWSCAN_ALL_IFACES)) 
            /* Additional check against infinite recursion */
            /* Got it, print info about this interface */
            (*fn)(fd, skfd, name, args, count);
        }

      fclose(fh);
    }
  else
    {
      /* Get list of configured devices using "traditional" way */
      ifc.ifc_len = sizeof(buff);
      ifc.ifc_buf = buff;
      if(ioctl(skfd, SIOCGIFCONF, &ifc) < 0)
        {
          /* Bad ending */
          fprintf(fd, " failed='1' error='SIOCGIFCONF: %s' />\n", 
              strerror(errno));
          return;
        }
      /*Good ending */
      fprintf(fd, ">\n");

      ifr = ifc.ifc_req;

      /* Print them */
      for(i = ifc.ifc_len / sizeof(struct ifreq); --i >= 0; ifr++)
        if (strcmp(ifr->ifr_name, IWSCAN_ALL_IFACES))
          (*fn)(fd, skfd, ifr->ifr_name, args, count);
    }

  /* And print closing tag */
  fprintf(fd, "</Scan>\n");

}

/*------------------------------------------------------------------*/
/*
 * Perform a scanning on one device
 */
int
iws_print_scanning_info_xml(FILE *	fd,
                    int         skfd,
                    char *      ifname,
                    char *      args[],         /* Command line args */
                    int         count)          /* Args count */
{
  struct iwreq          wrq;
  struct iw_scan_req    scanopt;                /* Options for 'set' */
  int                   scanflags = 0;          /* Flags for scan */
  unsigned char *       buffer = NULL;          /* Results */
  int                   buflen = IW_SCAN_MAX_DATA; /* Min for compat WE<17 */
  struct iw_range       range;
  int                   has_range;
  struct timeval        tv;                             /* Select timeout */
  int                   timeout = 15000000;             /* 15s */
  char                  stamp_buf[IWS_TIMESTAMP_LENGTH];
  struct timeval	tv_start, tv_end;
  double                duration;

  /* Avoid "Unused parameter" warning */
  args = args; count = count;

  /* Debugging stuff */
  if((IW_EV_LCP_PK2_LEN != IW_EV_LCP_PK_LEN) || (IW_EV_POINT_PK2_LEN != IW_EV_POINT_PK_LEN))
    {
      fprintf(stderr, "*** Please report to jt@hpl.hp.com your platform details\n");
      fprintf(stderr, "*** and the following line :\n");
      fprintf(stderr, "*** IW_EV_LCP_PK2_LEN = %zu ; IW_EV_POINT_PK2_LEN = %zu\n\n",
              IW_EV_LCP_PK2_LEN, IW_EV_POINT_PK2_LEN);
    }

  if (!strcmp(ifname, IWSCAN_ALL_IFACES))
    {
      iw_enum_devices_xml(fd, skfd, iws_print_scanning_info_xml, args, count);
      return 0;
    }

  /* Set begin time */
  /*time(&stamp);*/
  iws_format_timestamp((char *) &stamp_buf);
  gettimeofday(&tv_start, NULL);
  
  /* Convert stamp to str */ 
  /*stamp_ti = gmtime(&stamp);
  strftime(stamp_buf, TIMELEN, TIMEFMT, stamp_ti);*/

  /* Get range stuff */
  has_range = (iw_get_range_info(skfd, ifname, &range) >= 0);

  /* Check if the interface could support scanning. */
  if((!has_range) || (range.we_version_compiled < 14))
    {
      fprintf(fd, FAILMSG,
        ifname, "Interface does not support scanning", (char *) &stamp_buf);
      return -1;
    }

  /* Init timeout value -> 250ms between set and first get */
  tv.tv_sec = 0;
  tv.tv_usec = 250000;

  /* Clean up set args */
  memset(&scanopt, 0, sizeof(scanopt));

  /* Parse command line arguments and extract options.
   * Note : when we have enough options, we should use the parser
   * from iwconfig... */
  while(count > 0)
    {
      /* One arg is consumed (the option name) */
      count--;

      /*
       * Check for Active Scan (scan with specific essid)
       */
      if(!strcmp(args[0], "essid"))
        {
          if(count < 1)
            {
              fprintf(fd, "<Interface name='%s' failed='1' "
                  "error='Too few arguments for scanning option [%s]' "
                  "stamp='%s' />\n", ifname, args[0], (char *) &stamp_buf);
              return -1;
            }
          args++;
          count--;

          /* Store the ESSID in the scan options */
          scanopt.essid_len = strlen(args[0]);
          memcpy(scanopt.essid, args[0], scanopt.essid_len);
          /* Initialise BSSID as needed */
          if(scanopt.bssid.sa_family == 0)
            {
              scanopt.bssid.sa_family = ARPHRD_ETHER;
              memset(scanopt.bssid.sa_data, 0xff, ETH_ALEN);
            }
          /* Scan only this ESSID */
          scanflags |= IW_SCAN_THIS_ESSID;
        }
      else
      /* Check for last scan result (do not trigger scan) */
      if(!strcmp(args[0], "last"))
        {
          /* Hack */
          scanflags |= IW_SCAN_HACK;
        }
      else
      /* Skip iwscan and iwscand options */
      if (!strcmp(args[0], "to") || !strcmp(args[0], "pid"))
      /* If it's filename, we don't care */
	{
          /* And if there is filename, let's skip it */
          if (count > 0)
            {
              args++;
              count--;
            }
        }
      else
      if (!strcmp(args[0], "safe") || !strcmp(args[0], "nonblock"))
        { /* Do nothing */ }
      else
        {
          fprintf(fd, "<Interface name='%s' failed='1' error='Invalid "
              "scanning option [%s]' stamp='%s' />\n", ifname, args[0],
              (char *) &stamp_buf);
          return -1;
        }

      /* Next arg */
      args++;
    }

  /* Check if we have scan options */
  if(scanflags)
    {
      wrq.u.data.pointer = (caddr_t) &scanopt;
      wrq.u.data.length = sizeof(scanopt);
      wrq.u.data.flags = scanflags;
    }
  else
    {
      wrq.u.data.pointer = NULL;
      wrq.u.data.flags = 0;
      wrq.u.data.length = 0;
    }

   /* If only 'last' was specified on command line, don't trigger a scan */
  if(scanflags == IW_SCAN_HACK)
    {
      /* Skip waiting */
      tv.tv_usec = 0;
    }
  else
    {
      /* Initiate Scanning */
      if(iw_set_ext(skfd, ifname, SIOCSIWSCAN, &wrq) < 0)
        {
          /* If "last" not specified in command line, don't try to read left-over
	   * scan results. IMHO, it's better to print explicit error instead. */
          /*if((errno != EPERM) || (scanflags != 0))
            {*/
              fprintf(fd, FAILMSG, ifname, strerror(errno), 
                  (char *) &stamp_buf);
              return -1;
          /*  }
          tv.tv_usec = 0; */
        }
    }
  timeout -= tv.tv_usec;

  /* Forever */
  while(1)
    {
      fd_set            rfds;           /* File descriptors for select */
      int               last_fd;        /* Last fd */
      int               ret;

      /* Guess what ? We must re-generate rfds each time */
      FD_ZERO(&rfds);
      last_fd = -1;

      /* In here, add the rtnetlink fd in the list */

      /* Wait until something happens */
      ret = select(last_fd + 1, &rfds, NULL, NULL, &tv);

      /* Check if there was an error */
      if(ret < 0)
        {
          if(errno == EAGAIN || errno == EINTR)
            continue;
          fprintf(fd, FAILMSG, ifname, "Unhandled signal", 
              (char *) &stamp_buf);
          return(-1);
        }

      /* Check if there was a timeout */
      if(ret == 0)
        {
          unsigned char *       newbuf;

        realloc:
          /* (Re)allocate the buffer - realloc(NULL, len) == malloc(len) */
          newbuf = realloc(buffer, buflen);
          if(newbuf == NULL)
            {
              if(buffer)
                free(buffer);
              fprintf(fd, FAILMSG, ifname, "Allocation failed", 
                  (char *) &stamp_buf);
              return -1;
            }
          buffer = newbuf;

          /* Try to read the results */
          wrq.u.data.pointer = buffer;
          wrq.u.data.flags = 0;
          wrq.u.data.length = buflen;
          if(iw_get_ext(skfd, ifname, SIOCGIWSCAN, &wrq) < 0)
            {
              /* Check if buffer was too small (WE-17 only) */
              if((errno == E2BIG) && (range.we_version_compiled > 16)
                 && (buflen < 0xFFFF))
                {
                  /* Some driver may return very large scan results, either
                   * because there are many cells, or because they have many
                   * large elements in cells (like IWEVCUSTOM). Most will
                   * only need the regular sized buffer. We now use a dynamic
                   * allocation of the buffer to satisfy everybody. Of course,
                   * as we don't know in advance the size of the array, we try
                   * various increasing sizes. Jean II */

                  /* Check if the driver gave us any hints. */
                  if(wrq.u.data.length > buflen)
                    buflen = wrq.u.data.length;
                  else
                    buflen *= 2;

                  /* wrq.u.data.length is 16 bits so max size is 65535 */
                  if(buflen > 0xFFFF)
                    buflen = 0xFFFF;

                  /* Try again */
                  goto realloc;
                }

              /* Check if results not available yet */
              if(errno == EAGAIN)
                {
                  /* Restart timer for only 100ms*/
                  tv.tv_sec = 0;
                  tv.tv_usec = 100000;
                  timeout -= tv.tv_usec;
                  if(timeout > 0)
                    continue;   /* Try again later */
                }

              /* Bad error */
              free(buffer);
              fprintf(fd, FAILMSG, ifname, strerror(errno), 
                  (char *) &stamp_buf);
              return -2;
            }
          else
            /* We have the results, go to process them */
            break;
        }

      /* In here, check if event and event type
       * if scan event, read results. All errors bad & no reset timeout */
    }

  /* Set end time */
  gettimeofday(&tv_end, NULL);
  duration = tv_end.tv_sec - tv_start.tv_sec +
    (tv_end.tv_usec - tv_start.tv_usec)*1e-6;

  if(wrq.u.data.length)
    {
      struct iw_event           iwe;
      struct stream_descr       stream;
      struct iwscan_state_ext   state = { .ap_num = 1, .val_index = 0, 
                                        .extra_ie = 0};
      int                       ret;

      fprintf(fd, "<Interface name='%s' failed='0' stamp='%s' "
        "duration='PT%.2fS'>\n", ifname, (char *) &stamp_buf, duration);
      iw_init_event_stream(&stream, (char *) buffer, wrq.u.data.length);
      do
        {
          /* Extract an event and print it */
          ret = iw_extract_event_stream(&stream, &iwe,
                                        range.we_version_compiled);
          if(ret > 0)
            print_scanning_token_xml(fd, &stream, &iwe, &state,
                                 &range, has_range);
        }
      while(ret > 0);
      fprintf(fd, "\n</AP>\n</Interface>\n");
    }
  else
    fprintf(fd, "<Interface name='%s' failed='0' stamp='%s' "
      "duration='PT%.2fS' />\n", ifname, (char *) &stamp_buf, duration);

  free(buffer);
  return(0);
}

