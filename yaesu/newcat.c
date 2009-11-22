/*
 * hamlib - (C) Frank Singleton 2000 (javabear at users.sourceforge.net)
 *              and the Hamlib Group (hamlib-developer at lists.sourceforge.net)
 *
 * newcat.c - (C) Nate Bargmann 2007 (n0nb at arrl.net)
 *            (C) Stephane Fillod 2008
 *            (C) Terry Embry 2008-2009
 *
 * This shared library provides an API for communicating
 * via serial interface to any newer Yaesu radio using the
 * "new" text CAT interface.
 *
 * Models this code aims to support are FTDX-9000*, FT-2000,
 * FT-950, FT-450.  Much testing remains.  -N0NB
 *
 *
 * $Id: newcat.c,v 1.56 2009-02-26 23:04:26 mrtembry Exp $
 *
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
 *
 */


#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>  /* String function definitions */
#include <unistd.h>  /* UNIX standard function definitions */

#include "hamlib/rig.h"
#include "iofunc.h"
#include "newcat.h"

/* global variables */
static const char cat_term = ';';             /* Yaesu command terminator */
static const char cat_unknown_cmd[] = "?;";   /* Yaesu ? */

/* Internal Backup and Restore VFO Memory Channels */
#define NC_MEM_CHANNEL_NONE  2012
#define NC_MEM_CHANNEL_VFO_A 2013
#define NC_MEM_CHANNEL_VFO_B 2014

/* ID 0310 == 310, Must drop leading zero */
typedef enum nc_rigid_e {
    NC_RIGID_NONE            = 0,
    NC_RIGID_FT450           = 241,
    NC_RIGID_FT950           = 310,
    NC_RIGID_FT2000          = 251,
    NC_RIGID_FT2000D         = 252,
    NC_RIGID_FTDX9000D       = 101,
    NC_RIGID_FTDX9000Contest = 102,
    NC_RIGID_FTDX9000MP      = 103
} nc_rigid_t;


/*
 * The following table defines which commands are valid for any given
 * rig supporting the "new" CAT interface.
 */

typedef struct _yaesu_newcat_commands {
    char                *command;
    ncboolean           ft450;
    ncboolean           ft950;
    ncboolean           ft2000;
    ncboolean           ft9000;
} yaesu_newcat_commands_t;

/*
 * NOTE: The following table must be in alphabetical order by the
 * command.  This is because it is searched using a binary search
 * to determine whether or not a command is valid for a given rig.
 *
 * The list of supported commands is obtained from the rig's operator's
 * or CAT programming manual.
 *
 */
static const yaesu_newcat_commands_t valid_commands[] = {
    /*   Command    FT-450  FT-950  FT-2000 FT-9000 */
    {"AB",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"AC",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"AG",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"AI",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"AM",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"AN",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"BC",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"BD",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"BI",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"BP",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"BS",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"BU",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"BY",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"CH",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"CN",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"CO",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"CS",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"CT",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"DA",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"DN",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"DP",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"DS",      TRUE,   FALSE,  TRUE,   TRUE    },
    {"ED",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"EK",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"EU",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"EX",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"FA",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"FB",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"FK",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"FR",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"FS",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"FT",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"GT",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"ID",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"IF",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"IS",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"KM",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"KP",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"KR",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"KS",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"KY",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"LK",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"LM",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"MA",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"MC",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"MD",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"MG",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"MK",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"ML",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"MR",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"MS",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"MW",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"MX",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"NA",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"NB",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"NL",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"NR",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"OI",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"OS",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"PA",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"PB",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"PC",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"PL",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"PR",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"PS",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"QI",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"QR",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"QS",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"RA",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"RC",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"RD",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"RF",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"RG",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"RI",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"RL",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"RM",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"RO",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"RP",      TRUE,   FALSE,  FALSE,  FALSE   },
    {"RS",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"RT",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"RU",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"SC",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"SD",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"SF",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"SH",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"SM",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"SQ",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"ST",      TRUE,   FALSE,  FALSE,  FALSE   },
    {"SV",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"TS",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"TX",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"UL",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"UP",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"VD",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"VF",      FALSE,  TRUE,   TRUE,   TRUE    },
    {"VG",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"VM",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"VR",      TRUE,   FALSE,  FALSE,  FALSE   },
    {"VS",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"VV",      TRUE,   FALSE,  FALSE,  FALSE   },
    {"VX",      TRUE,   TRUE,   TRUE,   TRUE    },
    {"XT",      FALSE,  TRUE,   TRUE,   TRUE    },
};
int                     valid_commands_count = sizeof(valid_commands) / sizeof(yaesu_newcat_commands_t);

/*
 * future - private data
 *
 * FIXME: Does this need to be exposed to the application/frontend through
 * rig_caps.priv?  I'm guessing not since it's private to the backend.  -N0NB
 */

struct newcat_priv_data {
    unsigned int        read_update_delay;              /* depends on pacing value */
    vfo_t               current_vfo;                    /* active VFO from last cmd */
    char                cmd_str[NEWCAT_DATA_LEN];       /* command string buffer */
    char                ret_data[NEWCAT_DATA_LEN];      /* returned data--max value, most are less */
    int                 current_mem;                    /* private memory channel number */
    int                 rig_id;                         /* rig id from CAT Command ID; */
};

typedef struct newcat_cmd_data {
    char                cmd_str[NEWCAT_DATA_LEN];       /* command string buffer */
    char                ret_data[NEWCAT_DATA_LEN];      /* returned data--max value, most are less */
} newcat_cmd_data_t;

/* NewCAT Internal Functions */
static ncboolean newcat_is_rig(RIG * rig, rig_model_t model);
static int newcat_get_tx_vfo(RIG * rig, vfo_t * tx_vfo);
static int newcat_set_tx_vfo(RIG * rig, vfo_t tx_vfo);
static int newcat_get_rx_vfo(RIG * rig, vfo_t * rx_vfo);
static int newcat_set_rx_vfo(RIG * rig, vfo_t rx_vfo);
static int newcat_set_vfo_from_alias(RIG * rig, vfo_t * vfo);
static int newcat_scale_float(int scale, float fval);
static int newcat_get_rx_bandwidth(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t *width);
static int newcat_set_rx_bandwidth(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width);
static int newcat_set_narrow(RIG * rig, vfo_t vfo, ncboolean narrow);
static int newcat_get_narrow(RIG * rig, vfo_t vfo, ncboolean * narrow);
static int newcat_set_faststep(RIG * rig, ncboolean fast_step);
static int newcat_get_faststep(RIG * rig, ncboolean * fast_step);
static int newcat_get_rigid(RIG * rig);
static int newcat_get_vfo_mode(RIG * rig, vfo_t * vfo_mode);
static int newcat_set_cmd(RIG * rig, newcat_cmd_data_t * cmd);
static int newcat_get_cmd(RIG * rig, newcat_cmd_data_t * cmd);
static int newcat_set_any_mem(RIG * rig, vfo_t vfo, int ch);
static int newcat_set_any_channel(RIG * rig, const channel_t * chan);
static int newcat_vfomem_toggle(RIG * rig);

/*
 * ************************************
 *
 * Hamlib API functions
 *
 * ************************************
 */

/*
 * rig_init
 *
 */

int newcat_init(RIG *rig) {
    struct newcat_priv_data *priv;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;

    priv = (struct newcat_priv_data *)malloc(sizeof(struct newcat_priv_data));
    if (!priv)                                  /* whoops! memory shortage! */
        return -RIG_ENOMEM;

    /* TODO: read pacing from preferences */
    //    priv->pacing = NEWCAT_PACING_DEFAULT_VALUE; /* set pacing to minimum for now */
    priv->read_update_delay = NEWCAT_DEFAULT_READ_TIMEOUT; /* set update timeout to safe value */

    //    priv->current_vfo =  RIG_VFO_MAIN;          /* default to whatever */
    priv->current_vfo = RIG_VFO_A;

    rig->state.priv = (void *)priv;

    priv->rig_id = NC_RIGID_NONE;
    priv->current_mem = NC_MEM_CHANNEL_NONE;

    return RIG_OK;
}


/*
 * rig_cleanup
 *
 * the serial port is closed by the frontend
 *
 */

int newcat_cleanup(RIG *rig) {

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;

    if (rig->state.priv)
        free(rig->state.priv);
    rig->state.priv = NULL;

    return RIG_OK;
}


/*
 * rig_open
 *
 * New CAT does not support pacing
 *
 */

int newcat_open(RIG *rig) {
    struct rig_state *rig_s;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;

    //    priv = (struct newcat_priv_data *)rig->state.priv;
    rig_s = &rig->state;

    rig_debug(RIG_DEBUG_TRACE, "%s: write_delay = %i msec\n",
            __func__, rig_s->rigport.write_delay);
    rig_debug(RIG_DEBUG_TRACE, "%s: post_write_delay = %i msec\n",
            __func__, rig_s->rigport.post_write_delay);

    return RIG_OK;
}


/*
 * rig_close
 *
 */

int newcat_close(RIG *rig) {

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;

    return RIG_OK;
}


/*
 * rig_set_freq
 *
 * Set frequency for a given VFO
 * RIG_TARGETABLE_VFO
 * Does not SET priv->current_vfo
 *
 */

int newcat_set_freq(RIG *rig, vfo_t vfo, freq_t freq) {
    const struct rig_caps *caps;
    struct newcat_priv_data *priv;
    struct rig_state *state;
    char c;
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "FA"))
        return -RIG_ENAVAIL;

    priv = (struct newcat_priv_data *)rig->state.priv;
    caps = rig->caps;
    state = &rig->state;

    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed freq = %"PRIfreq" Hz\n", __func__, freq);

    /* additional debugging */
    rig_debug(RIG_DEBUG_TRACE, "%s: R2 minimum freq = %"PRIfreq" Hz\n", __func__, caps->rx_range_list2[0].start);
    rig_debug(RIG_DEBUG_TRACE, "%s: R2 maximum freq = %"PRIfreq" Hz\n", __func__, caps->rx_range_list2[0].end);

    if (freq < caps->rx_range_list1[0].start || freq > caps->rx_range_list1[0].end ||
            freq < caps->rx_range_list2[0].start || freq > caps->rx_range_list2[0].end)
        return -RIG_EINVAL;

    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    switch (vfo) {
        case RIG_VFO_A:
            c = 'A';
            break;
        case RIG_VFO_B:
            c = 'B';
            break;
        case RIG_VFO_MEM:
            c = 'A';
            break;
        default:
            return -RIG_ENIMPL;             /* Only VFO_A or VFO_B are valid */
    }

    // W1HKJ
    // creation of the priv structure guarantees that the string can be NEWCAT_DATA_LEN
    // bytes in length.  the snprintf will only allow (NEWCAT_DATA_LEN - 1) chars
    // followed by the NULL terminator.
    // CAT command string for setting frequency requires that 8 digits be sent
    // including leading fill zeros

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "F%c%08d%c", c, (int)freq, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
 
    return err;
}


/*
 * rig_get_freq
 *
 * Return Freq for a given VFO
 * RIG_TARGETABLE_FREQ
 * Does not SET priv->current_vfo
 *
 */
int newcat_get_freq(RIG *rig, vfo_t vfo, freq_t *freq) {
    char command[3];
    struct newcat_priv_data *priv;
    struct rig_state *state;
    char c;
    int err;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);
    rig_debug(RIG_DEBUG_TRACE, "%s: passed vfo = 0x%02x\n", __func__, vfo);

    if (!newcat_valid_command(rig, "FA"))
        return -RIG_ENAVAIL;

    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    switch(vfo) {
        case RIG_VFO_A:
            c = 'A';
            break;
        case RIG_VFO_B:
            c = 'B';
            break;
        case RIG_VFO_MEM:
            c = 'A';
            break;
        default:
            return -RIG_EINVAL;         /* sorry, unsupported VFO */
    }

    /* Build the command string */
    snprintf(command, sizeof(command), "F%c", c);
    if (!newcat_valid_command(rig, command))
        return -RIG_ENAVAIL;
    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", command, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

    /* get freq */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data),
            &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s\n",
            __func__, err, priv->ret_data);

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n",
                __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get FREQ\n");
        return RIG_OK;
    }

    /* convert the read frequency string into freq_t and store in *freq */
    sscanf(priv->ret_data+2, "%"SCNfreq, freq);

    rig_debug(RIG_DEBUG_TRACE,
            "%s: freq = %"PRIfreq" Hz for vfo 0x%02x\n", __func__, freq, vfo);

    return RIG_OK;
}


int newcat_set_mode(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;


    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "MD"))
        return -RIG_ENAVAIL;


    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "MD0x%c", cat_term);

    /* FT9000 RIG_TARGETABLE_MODE (mode and width) */
    /* FT2000 mode only */
    if (newcat_is_rig(rig, RIG_MODEL_FT9000) || newcat_is_rig(rig, RIG_MODEL_FT2000))
        priv->cmd_str[2] = (RIG_VFO_B == vfo) ? '1' : '0';

    rig_debug(RIG_DEBUG_VERBOSE,"%s: generic mode = %x \n",
            __func__, mode);

    if (RIG_PASSBAND_NORMAL == width)
        width = rig_passband_normal(rig, mode);

    switch(mode) {
        case RIG_MODE_LSB:
            priv->cmd_str[3] = '1';
            break;
        case RIG_MODE_USB:
            priv->cmd_str[3] = '2';
            break;
        case RIG_MODE_CW:
            priv->cmd_str[3] = '3';
            break;
        case RIG_MODE_AM:
            priv->cmd_str[3] = '5';
            break;
        case RIG_MODE_RTTY:
            priv->cmd_str[3] = '6';
            break;
        case RIG_MODE_CWR:
            priv->cmd_str[3] = '7';
            break;
        case RIG_MODE_PKTLSB:   /* FT450 USER-L */
            priv->cmd_str[3] = '8';
            break;
        case RIG_MODE_RTTYR:
            priv->cmd_str[3] = '9';
            break;
        case RIG_MODE_PKTFM:
            priv->cmd_str[3] = 'A';
            break;
        case RIG_MODE_FM:
            priv->cmd_str[3] = '4';
            break;
        case RIG_MODE_PKTUSB:   /* FT450 USER-U */
            priv->cmd_str[3] = 'C';
            break;
        default:
            return -RIG_EINVAL;
    }

    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    if (RIG_PASSBAND_NORMAL == width)
        width = rig_passband_normal(rig, mode);

    /* Set width after mode has been set */
    err = newcat_set_rx_bandwidth(rig, vfo, mode, width);

    return err;
}


int newcat_get_mode(RIG *rig, vfo_t vfo, rmode_t *mode, pbwidth_t *width)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    char c;
    int err;
    ncboolean narrow;
    char main_sub_vfo = '0';

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "MD"))
        return -RIG_ENAVAIL;

    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    if (newcat_is_rig(rig, RIG_MODEL_FT9000) || newcat_is_rig(rig, RIG_MODEL_FT2000))
        main_sub_vfo = RIG_VFO_B == vfo ? '1' : '0';

    /* Build the command string */
    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "MD%c%c", main_sub_vfo, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Get MODE */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data),
            &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n",
                __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s\n",
            __func__, err, priv->ret_data);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get MODE\n");
        return RIG_OK;
    }

    /*
     * The current mode value is a digit '0' ... 'C'
     * embedded at ret_data[3] in the read string.
     */
    c = priv->ret_data[3];

    /* default, unless set otherwise */
    *width = RIG_PASSBAND_NORMAL;

    switch (c) {
        case '1':
            *mode = RIG_MODE_LSB;
            break;
        case '2':
            *mode = RIG_MODE_USB;
            break;
        case '3':
            *mode = RIG_MODE_CW;
            break;
        case '4':
            *mode = RIG_MODE_FM;
            err = newcat_get_narrow(rig, vfo, &narrow);
            if (narrow == TRUE)
                *width = rig_passband_narrow(rig, *mode);
            else 
                *width = rig_passband_normal(rig, *mode);
            return err;
        case '5':
            *mode = RIG_MODE_AM;
            err = newcat_get_narrow(rig, vfo, &narrow);
            if (narrow == TRUE)
                *width = rig_passband_narrow(rig, *mode);
            else 
                *width = rig_passband_normal(rig, *mode);
            return err;
        case '6':
            *mode = RIG_MODE_RTTY;
            break;
        case '7':
            *mode = RIG_MODE_CWR;
            break;
        case '8':
            *mode = RIG_MODE_PKTLSB;    /* FT450 USER-L */
            break;
        case '9':
            *mode = RIG_MODE_RTTYR;
            break;
        case 'A':
            *mode = RIG_MODE_PKTFM;
            err = newcat_get_narrow(rig, vfo, &narrow);
            if (narrow == TRUE)
                *width = rig_passband_narrow(rig, *mode);
            else 
                *width = rig_passband_normal(rig, *mode);
            return err;
        case 'B':
            *mode = RIG_MODE_FM;       /* narrow */
            *width = rig_passband_narrow(rig, *mode);
            return RIG_OK;
        case 'C':
            *mode = RIG_MODE_PKTUSB;    /* FT450 USER-U */
            break;
        case 'D':
            *mode = RIG_MODE_AM;       /* narrow, FT950 */
            *width = rig_passband_narrow(rig, *mode);
            return RIG_OK;
        default:
            return -RIG_EPROTO;
    }

    if (RIG_PASSBAND_NORMAL == *width)
        *width = rig_passband_normal(rig, *mode);

    err = newcat_get_rx_bandwidth(rig, vfo, *mode, width);
    if (err < 0)
        return err;

    return RIG_OK;
}

/*
 * rig_set_vfo
 *
 * set vfo and store requested vfo for later RIG_VFO_CURR
 * requests.
 *
 */

int newcat_set_vfo(RIG *rig, vfo_t vfo) {
    struct newcat_priv_data *priv;
    struct rig_state *state;
    char c;
    int err, mem;
    vfo_t vfo_mode;
    char command[] = "VS";
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_TRACE, "%s: called, passed vfo = 0x%02x\n", __func__, vfo);

    if (!newcat_valid_command(rig, command)) {
        err = newcat_set_rx_vfo(rig, vfo);       /* Try set with "FR" */
        return err;
    }

    err = newcat_set_vfo_from_alias(rig, &vfo);   /* passes RIG_VFO_MEM, RIG_VFO_A, RIG_VFO_B */
    if (err < 0)
        return err;

    switch(vfo) {
        case RIG_VFO_A:
        case RIG_VFO_B:    
            if (vfo == RIG_VFO_B)
                c = '1';
            else
                c = '0';

            err = newcat_get_vfo_mode(rig, &vfo_mode);
            if (vfo_mode == RIG_VFO_MEM) {
                priv->current_mem = NC_MEM_CHANNEL_NONE;
                priv->current_vfo = RIG_VFO_A;
                err = newcat_vfomem_toggle(rig);
                return err;
            }
            break;
        case RIG_VFO_MEM:
            if (priv->current_mem == NC_MEM_CHANNEL_NONE) {
                /* Only works correctly for VFO A */
                if (priv->current_vfo == RIG_VFO_B)
                    return -RIG_ENTARGET;

                /* get current memory channel */
                err = newcat_get_mem(rig, vfo, &mem);
                if (err != RIG_OK)
                    return err;

                /* turn on memory channel */
                err = newcat_set_mem(rig, vfo, mem);
                if (err != RIG_OK)
                    return err;

                /* Set current_mem now */
                priv->current_mem = mem;
            }
            /* Set current_vfo now */
            priv->current_vfo = vfo;
            return RIG_OK;
        default:
            return -RIG_ENIMPL;         /* sorry, VFO not implemented */
    }

    /* Build the command string */
    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", command, c, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    priv->current_vfo = vfo;    /* if set_vfo worked, set current_vfo */ 

    rig_debug(RIG_DEBUG_TRACE, "%s: priv->current_vfo = 0x%02x\n", __func__, vfo);

    return RIG_OK;
}


/*
 * rig_get_vfo
 *
 * get current RX vfo/mem and store requested vfo for
 * later RIG_VFO_CURR requests plus pass the tested vfo/mem
 * back to the frontend.
 *
 */

int newcat_get_vfo(RIG *rig, vfo_t *vfo) {
    struct newcat_priv_data *priv;
    struct rig_state *state;
    char c;
    int err;
    vfo_t vfo_mode;
    char command[] = "VS";
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, command)) {
        err = newcat_get_rx_vfo(rig, vfo);       /* Try get with "FR" */
        return err;
    }

    /* Build the command string */
    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s;", command);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Get VFO */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data),
            &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n",
                __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, VFO value = %c\n",
            __func__, err, priv->ret_data, priv->ret_data[2]);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get VFO\n");
        *vfo = RIG_VFO_A;
        priv->current_vfo = RIG_VFO_A;
        return RIG_OK;
    }

    /*
     * The current VFO value is a digit ('0' or '1' ('A' or 'B' respectively))
     * embedded at ret_data[2] in the read string.
     */

    c = priv->ret_data[2];

    switch (c) {
        case '0':
            *vfo = RIG_VFO_A;
            break;
        case '1':
            *vfo = RIG_VFO_B;
            break;
        default:
            return -RIG_EPROTO;         /* sorry, wrong current VFO */
    }

    /* Check to see if RIG is in MEM mode */
    err = newcat_get_vfo_mode(rig, &vfo_mode);
    if (vfo_mode == RIG_VFO_MEM)
        *vfo = RIG_VFO_MEM;

    priv->current_vfo = *vfo;       /* set now */

    rig_debug(RIG_DEBUG_TRACE, "%s: set vfo = 0x%02x\n", __func__, *vfo);

    return RIG_OK;

}


int newcat_set_ptt(RIG *rig, vfo_t vfo, ptt_t ptt)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char txon[] = "TX1;";
    char txoff[] = "TX0;";

    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    if (!newcat_valid_command(rig, "TX"))
        return -RIG_ENAVAIL;

    switch(ptt) {
        case RIG_PTT_ON:
            err = write_block(&state->rigport, txon, strlen(txon));
            break;
        case RIG_PTT_OFF:
            err = write_block(&state->rigport, txoff, strlen(txoff));
            break;
        default:
            return -RIG_EINVAL;
    }
    return err;
}


int newcat_get_ptt(RIG * rig, vfo_t vfo, ptt_t * ptt)
{
    char c;
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    if (!newcat_valid_command(rig, "TX"))
        return -RIG_ENAVAIL;

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", "TX", cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Get PTT */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, PTT value = %c\n", __func__, err, priv->ret_data, priv->ret_data[2]);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get PTT\n");
        return RIG_OK;
    }

    c = priv->ret_data[2];
    switch (c) {
        case '0':                 /* FT-950 "TX OFF", Original Release Firmware */
            *ptt = RIG_PTT_OFF;
            break;
        case '1' :                /* Just because,    what the CAT Manual Shows */
        case '2' :                /* FT-950 Radio:    Mic, Dataport, CW "TX ON" */
        case '3' :                /* FT-950 CAT port: Radio in "TX ON" mode     [Not what the CAT Manual Shows] */
            *ptt = RIG_PTT_ON;
            break;
        default:
            return -RIG_EPROTO;
    }

    return RIG_OK;
}


int newcat_get_dcd(RIG * rig, vfo_t vfo, dcd_t * dcd)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_rptr_shift(RIG * rig, vfo_t vfo, rptr_shift_t rptr_shift)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char c;
    char command[] = "OS";
    char main_sub_vfo = '0';
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    if (!newcat_valid_command(rig, command))
        return -RIG_ENAVAIL;

    /* Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    if (newcat_is_rig(rig, RIG_MODEL_FT9000) || newcat_is_rig(rig, RIG_MODEL_FT2000))
        main_sub_vfo = RIG_VFO_B == vfo ? '1' : '0';

    switch (rptr_shift) {
        case RIG_RPT_SHIFT_NONE:
            c = '0';
            break;
        case RIG_RPT_SHIFT_PLUS:
            c = '1';
            break;
        case RIG_RPT_SHIFT_MINUS:
            c = '2';
            break;
        default:
            return -RIG_EINVAL;

    }

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c%c", command, main_sub_vfo, c, cat_term);
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));

    return err;
}


int newcat_get_rptr_shift(RIG * rig, vfo_t vfo, rptr_shift_t * rptr_shift)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char c;
    char command[] = "OS";
    char main_sub_vfo = '0';
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, command))
        return -RIG_ENAVAIL;

    /* Set Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    if (newcat_is_rig(rig, RIG_MODEL_FT9000) || newcat_is_rig(rig, RIG_MODEL_FT2000))
        main_sub_vfo = (RIG_VFO_B == vfo) ? '1' : '0';

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", command, main_sub_vfo, cat_term);
    /* Get Rptr Shift */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", 
                __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, Rptr Shift value = %c\n", __func__, err, priv->ret_data, priv->ret_data[3]);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get RPTR_SHIFT\n");
        return RIG_OK;
    }

    c = priv->ret_data[3];
    switch (c) {
        case '0':
            *rptr_shift = RIG_RPT_SHIFT_NONE;
            break; 
        case '1':
            *rptr_shift = RIG_RPT_SHIFT_PLUS;
            break;
        case '2':
            *rptr_shift = RIG_RPT_SHIFT_MINUS;
            break;
        default: 
            return -RIG_EINVAL;
    } 

    return RIG_OK;
}


int newcat_set_rptr_offs(RIG * rig, vfo_t vfo, shortfreq_t offs)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_get_rptr_offs(RIG * rig, vfo_t vfo, shortfreq_t * offs)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_split_freq(RIG * rig, vfo_t vfo, freq_t tx_freq)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_get_split_freq(RIG * rig, vfo_t vfo, freq_t * tx_freq)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_split_mode(RIG * rig, vfo_t vfo, rmode_t tx_mode, pbwidth_t tx_width)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_get_split_mode(RIG * rig, vfo_t vfo, rmode_t * tx_mode, pbwidth_t * tx_width)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_split_vfo(RIG * rig, vfo_t vfo, split_t split, vfo_t tx_vfo)
{
    int err;
    vfo_t rx_vfo;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    err = newcat_get_vfo(rig, &rx_vfo);  /* sync to rig current vfo */
    if (err != RIG_OK)
        return err;

    switch (split) {
        case RIG_SPLIT_OFF:
            err = newcat_set_tx_vfo(rig, vfo);
            if (err != RIG_OK)
                return err;

            if (rx_vfo != vfo) {
                err = newcat_set_vfo(rig, vfo);
                if (err != RIG_OK)
                    return err;
            }
            break;
        case RIG_SPLIT_ON:
            err = newcat_set_tx_vfo(rig, tx_vfo);
            if (err != RIG_OK)
                return err;

            if (rx_vfo != vfo) {
                err = newcat_set_vfo(rig, vfo);
                if (err != RIG_OK)
                    return err;
            }
            break;
        default:
            return -RIG_EINVAL;
    }

    return RIG_OK;
}


int newcat_get_split_vfo(RIG * rig, vfo_t vfo, split_t * split, vfo_t *tx_vfo)
{
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err != RIG_OK)
        return err;

    err = newcat_get_tx_vfo(rig, tx_vfo);
    if (err != RIG_OK)
        return err;

    if (*tx_vfo != vfo)
        *split = RIG_SPLIT_ON;
    else
        *split = RIG_SPLIT_OFF;

    rig_debug(RIG_DEBUG_TRACE, "SPLIT = %d, vfo = %d, TX_vfo = %d\n", *split, vfo, *tx_vfo);

    return RIG_OK;
}


int newcat_set_rit(RIG * rig, vfo_t vfo, shortfreq_t rit)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    if (!newcat_valid_command(rig, "RT"))
        return -RIG_ENAVAIL;

    if (rit > rig->caps->max_rit)
        rit = rig->caps->max_rit;   /* + */
    else if (abs(rit) > rig->caps->max_rit)
        rit = - rig->caps->max_rit;  /* - */

    if (rit == 0)
        snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RC%cRT0%c", cat_term, cat_term);
    else if (rit < 0)
        snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RC%cRD%04d%cRT1%c", cat_term, abs(rit), cat_term, cat_term);
    else
        snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RC%cRU%04d%cRT1%c", cat_term, abs(rit), cat_term, cat_term);

    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));

    return err;
}


int newcat_get_rit(RIG * rig, vfo_t vfo, shortfreq_t * rit)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    char *retval;
    char rit_on;
    int err;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    if (!newcat_valid_command(rig, "IF"))
        return -RIG_ENAVAIL;

    *rit = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", "IF", cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Get RIT */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, RIT value = %c\n", __func__, err, priv->ret_data, priv->ret_data[18]);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get RIT\n");
        return RIG_OK;
    }

    retval = priv->ret_data + 13;
    rit_on = retval[5];
    retval[5] = '\0';

    if (rit_on == '1')
        *rit = (shortfreq_t) atoi(retval);

    return RIG_OK;
}


int newcat_set_xit(RIG * rig, vfo_t vfo, shortfreq_t xit)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
   
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    if (!newcat_valid_command(rig, "XT"))
        return -RIG_ENAVAIL;

    if (xit > rig->caps->max_xit)
        xit = rig->caps->max_xit;   /* + */
    else if (abs(xit) > rig->caps->max_xit)
        xit = - rig->caps->max_xit;  /* - */

    if (xit == 0)
        snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RC%cXT0%c", cat_term, cat_term);
    else if (xit < 0)
        snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RC%cRD%04d%cXT1%c", cat_term, abs(xit), cat_term, cat_term);
    else
        snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RC%cRU%04d%cXT1%c", cat_term, abs(xit), cat_term, cat_term);

    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));

    return err;
}


int newcat_get_xit(RIG * rig, vfo_t vfo, shortfreq_t * xit)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    char *retval;
    char xit_on;
    int err;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    if (!newcat_valid_command(rig, "IF"))
        return -RIG_ENAVAIL;

    *xit = 0;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", "IF", cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Get XIT */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, XIT value = %c\n", __func__, err, priv->ret_data, priv->ret_data[19]);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get XIT\n");
        return RIG_OK;
    }

    retval = priv->ret_data + 13;
    xit_on = retval[6];
    retval[5] = '\0';

    if (xit_on == '1')
        *xit = (shortfreq_t) atoi(retval);

    return RIG_OK;
}


int newcat_set_ts(RIG * rig, vfo_t vfo, shortfreq_t ts)
{
    int err, i;
    pbwidth_t width;
    rmode_t mode;
    ncboolean ts_match;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    err = newcat_get_mode(rig, vfo, &mode, &width);
    if (err < 0)
        return err;

    /* assume 2 tuning steps per mode */
    for (i = 0, ts_match = FALSE; i < TSLSTSIZ && rig->caps->tuning_steps[i].ts; i++)
        if (rig->caps->tuning_steps[i].modes & mode) {
            if (ts <= rig->caps->tuning_steps[i].ts)
                err = newcat_set_faststep(rig, FALSE);
            else 
                err = newcat_set_faststep(rig, TRUE);

            if (err != RIG_OK)
                return err;
            ts_match = TRUE;
            break;
        }   /* if mode */

    rig_debug(RIG_DEBUG_TRACE, "ts_match = %d, i = %d, ts = %d\n", ts_match, i, ts);

    if (ts_match)
        return RIG_OK;
    else
        return -RIG_ENAVAIL;
}


int newcat_get_ts(RIG * rig, vfo_t vfo, shortfreq_t * ts)
{
    pbwidth_t width;
    rmode_t mode;
    int err, i;
    ncboolean ts_match;
    ncboolean fast_step = FALSE;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    err = newcat_get_mode(rig, vfo, &mode, &width);
    if (err < 0)
        return err;

    err = newcat_get_faststep(rig, &fast_step);
    if (err < 0)
        return err;

    /* assume 2 tuning steps per mode */
    for (i = 0, ts_match = FALSE; i < TSLSTSIZ && rig->caps->tuning_steps[i].ts; i++)
        if (rig->caps->tuning_steps[i].modes & mode) {
            if (fast_step == FALSE)
                *ts = rig->caps->tuning_steps[i].ts;
            else
                *ts = rig->caps->tuning_steps[i+1].ts;

            ts_match = TRUE;    
            break;
        }

    rig_debug(RIG_DEBUG_TRACE, "ts_match = %d, i = %d, i+1 = %d, *ts = %d\n", ts_match, i, i+1, *ts);

    if (ts_match)
        return RIG_OK;
    else
        return -RIG_ENAVAIL;
}


int newcat_set_dcs_code(RIG * rig, vfo_t vfo, tone_t code)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_get_dcs_code(RIG * rig, vfo_t vfo, tone_t * code)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_tone(RIG * rig, vfo_t vfo, tone_t tone)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_get_tone(RIG * rig, vfo_t vfo, tone_t * tone)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_ctcss_tone(RIG * rig, vfo_t vfo, tone_t tone)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    int i;
    ncboolean tone_match;
    char main_sub_vfo = '0';
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "CN"))
        return -RIG_ENAVAIL;

    if (!newcat_valid_command(rig, "CT"))
        return -RIG_ENAVAIL;

    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    if (newcat_is_rig(rig, RIG_MODEL_FT9000) || newcat_is_rig(rig, RIG_MODEL_FT2000))
        main_sub_vfo = (RIG_VFO_B == vfo) ? '1' : '0';

    for (i = 0, tone_match = FALSE; rig->caps->ctcss_list[i] != 0; i++)
        if (tone == rig->caps->ctcss_list[i]) { 
            tone_match = TRUE;
            break;
        }

    rig_debug(RIG_DEBUG_TRACE, "%s: tone = %d, tone_match = %d, i = %d", __func__, tone, tone_match, i);

    if (tone_match == FALSE && tone != 0)
        return -RIG_ENAVAIL;

    if (tone == 0)     /* turn off ctcss */
        snprintf(priv->cmd_str, sizeof(priv->cmd_str), "CT%c0%c", main_sub_vfo, cat_term);
    else {
        snprintf(priv->cmd_str, sizeof(priv->cmd_str), "CN%c%02d%cCT%c2%c", main_sub_vfo, i, cat_term, main_sub_vfo, cat_term);
    }
    
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));

    return err;
}


int newcat_get_ctcss_tone(RIG * rig, vfo_t vfo, tone_t * tone)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    int t;
    int ret_data_len;
    char *retlvl;
    char cmd[] = "CN";
    char main_sub_vfo = '0';
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, cmd))
        return -RIG_ENAVAIL;

    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    if (newcat_is_rig(rig, RIG_MODEL_FT9000) || newcat_is_rig(rig, RIG_MODEL_FT2000))
        main_sub_vfo = (RIG_VFO_B == vfo) ? '1' : '0';

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", cmd, main_sub_vfo, cat_term);

    /* Get CTCSS TONE */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s\n",
            __func__, err, priv->ret_data);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get CTCSS_TONE\n");
        return RIG_OK;
    }

    ret_data_len = strlen(priv->ret_data);

    /* skip command */
    retlvl = priv->ret_data + strlen(priv->cmd_str)-1;
    /* chop term */
    priv->ret_data[ret_data_len-1] = '\0';

    t = atoi(retlvl);   /*  tone index */

    if (t < 0 || t > 49)
        return -RIG_ENAVAIL;

    *tone = rig->caps->ctcss_list[t];

    return RIG_OK;
}


int newcat_set_dcs_sql(RIG * rig, vfo_t vfo, tone_t code)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_get_dcs_sql(RIG * rig, vfo_t vfo, tone_t * code)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_tone_sql(RIG * rig, vfo_t vfo, tone_t tone)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_get_tone_sql(RIG * rig, vfo_t vfo, tone_t * tone)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_ctcss_sql(RIG * rig, vfo_t vfo, tone_t tone)
{
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    err = newcat_set_ctcss_tone(rig, vfo, tone);
    if (err != RIG_OK)
        return err;

    /* Change to sql */
    if (tone) {      
        err = newcat_set_func(rig, vfo, RIG_FUNC_TSQL, TRUE);
        if (err != RIG_OK)
            return err;
    }

    return RIG_OK;
}


int newcat_get_ctcss_sql(RIG * rig, vfo_t vfo, tone_t * tone)
{
    int err;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    err = newcat_get_ctcss_tone(rig, vfo, tone);

    return err;
}


int newcat_power2mW(RIG * rig, unsigned int *mwpower, float power, freq_t freq, rmode_t mode)
{
    int rig_id;
    
    rig_id = newcat_get_rigid(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (rig_id) {
        case NC_RIGID_FT450:
            /* 100 Watts */
            *mwpower = power * 100000;
            rig_debug(RIG_DEBUG_TRACE, "case FT450 - rig_id = %d, *mwpower = %d\n", rig_id, *mwpower);
            break;
        case NC_RIGID_FT950:
            /* 100 Watts */
            *mwpower = power * 100000;      /* 0..100 Linear scale */
            rig_debug(RIG_DEBUG_TRACE, "case FT950 - rig_id = %d, power = %f, *mwpower = %d\n", rig_id, power, *mwpower);
            break;
        case NC_RIGID_FT2000:
            /* 100 Watts */
            *mwpower = power * 100000;
            rig_debug(RIG_DEBUG_TRACE, "case FT2000 - rig_id = %d, *mwpower = %d\n", rig_id, *mwpower);
            break;
        case NC_RIGID_FT2000D:
            /* 200 Watts */
            *mwpower = power * 200000;
            rig_debug(RIG_DEBUG_TRACE, "case FT2000D - rig_id = %d, *mwpower = %d\n", rig_id, *mwpower);
            break;
        case NC_RIGID_FTDX9000D:
            /* 200 Watts */
            *mwpower = power * 200000;
            rig_debug(RIG_DEBUG_TRACE, "case FTDX9000D - rig_id = %d, *mwpower = %d\n", rig_id, *mwpower);
            break;
        case NC_RIGID_FTDX9000Contest:
            /* 200 Watts */
            *mwpower = power * 200000;
            rig_debug(RIG_DEBUG_TRACE, "case FTDX9000Contest - rig_id = %d, *mwpower = %d\n", rig_id, *mwpower);
            break;
        case NC_RIGID_FTDX9000MP:
            /* 400 Watts */
            *mwpower = power * 400000;
            rig_debug(RIG_DEBUG_TRACE, "case FTDX9000MP - rig_id = %d, *mwpower = %d\n", rig_id, *mwpower);
            break;
        default:
            /* 100 Watts */
            *mwpower = power * 100000;
            rig_debug(RIG_DEBUG_TRACE, "default - rig_id = %d, *mwpower = %d\n", rig_id, *mwpower);
    }

    return RIG_OK;
}


int newcat_mW2power(RIG * rig, float *power, unsigned int mwpower, freq_t freq, rmode_t mode)
{
    int rig_id;
    
    rig_id = newcat_get_rigid(rig);

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    switch (rig_id) {
        case NC_RIGID_FT450:
            /* 100 Watts */
            *power = mwpower / 100000.0;
            rig_debug(RIG_DEBUG_TRACE, "case FT450 - rig_id = %d, *power = %f\n", rig_id, *power);
            break;
        case NC_RIGID_FT950:
            /* 100 Watts */
            *power = mwpower / 100000.0;      /* 0..100 Linear scale */
            rig_debug(RIG_DEBUG_TRACE, "case FT950 - rig_id = %d, mwpower = %d, *power = %f\n", rig_id, mwpower, *power);
            break;
        case NC_RIGID_FT2000:
            /* 100 Watts */
            *power = mwpower / 100000.0;
            rig_debug(RIG_DEBUG_TRACE, "case FT2000 - rig_id = %d, *power = %f\n", rig_id, *power);
            break;
        case NC_RIGID_FT2000D:
            /* 200 Watts */
            *power = mwpower / 200000.0;
            rig_debug(RIG_DEBUG_TRACE, "case FT2000D - rig_id = %d, *power = %f\n", rig_id, *power);
            break;
        case NC_RIGID_FTDX9000D:
            /* 200 Watts */
            *power = mwpower / 200000.0;
            rig_debug(RIG_DEBUG_TRACE, "case FTDX9000D - rig_id = %d, *power = %f\n", rig_id, *power);
            break;
        case NC_RIGID_FTDX9000Contest:
            /* 200 Watts */
            *power = mwpower / 200000.0;
            rig_debug(RIG_DEBUG_TRACE, "case FTDX9000Contest - rig_id = %d, *power = %f\n", rig_id, *power);
            break;
        case NC_RIGID_FTDX9000MP:
            /* 400 Watts */
            *power = mwpower / 400000.0;
            rig_debug(RIG_DEBUG_TRACE, "case FTDX9000MP - rig_id = %d, *power = %f\n", rig_id, *power);
            break;
        default:
            /* 100 Watts */
            *power = mwpower / 100000.0;
            rig_debug(RIG_DEBUG_TRACE, "default - rig_id = %d, *power = %f\n", rig_id, *power);
    }

    return RIG_OK;
}


int newcat_set_powerstat(RIG * rig, powerstat_t status)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char ps;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "PS"))
        return -RIG_ENAVAIL;

    switch (status) {
        case RIG_POWER_ON:
            ps = '1';
            break;
        case RIG_POWER_OFF:
        case RIG_POWER_STANDBY:
            ps = '0';
            break;
        default:
            return -RIG_ENAVAIL;
    }

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "PS%c%c", ps, cat_term);
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    // delay 1.5 seconds
    usleep(1500000);
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    
    return err;
}


/*
 *  This functions returns an error if the rig is off,  dah
 */
int newcat_get_powerstat(RIG * rig, powerstat_t * status)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char ps;
    char command[] = "PS";
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    *status = RIG_POWER_OFF;

    if (!newcat_valid_command(rig, command))
        return -RIG_ENAVAIL;

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", command, cat_term);
    /* Get Power status */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, PS value = %c\n", __func__, err, priv->ret_data, priv->ret_data[2]);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get PS\n");
        return RIG_OK;
    }

    ps = priv->ret_data[2];
    switch (ps) {
        case '1':
            *status = RIG_POWER_ON;
            break;
        case '0': 
            *status = RIG_POWER_OFF;
            break;
        default:
            return -RIG_ENAVAIL;
    }

    return RIG_OK;
}


int newcat_reset(RIG * rig, reset_t reset)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_ant(RIG * rig, vfo_t vfo, ant_t ant)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char which_ant;
    char command[] = "AN";
    char main_sub_vfo = '0';
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    if (!newcat_valid_command(rig, command))
        return -RIG_ENAVAIL;

    /* Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    if (newcat_is_rig(rig, RIG_MODEL_FT9000))
        main_sub_vfo = RIG_VFO_B == vfo ? '1' : '0';

    switch (ant) {
        case RIG_ANT_1:
            which_ant = '1';
            break;
        case RIG_ANT_2:
            which_ant = '2';
            break;
        case RIG_ANT_3:
            if (newcat_is_rig(rig, RIG_MODEL_FT950)) /* FT2000 also */
                return -RIG_EINVAL;
            which_ant = '3';
            break;
        case RIG_ANT_4:
            if (newcat_is_rig(rig, RIG_MODEL_FT950))
                return -RIG_EINVAL;
            which_ant = '4';
            break;
        case RIG_ANT_5:
            if (newcat_is_rig(rig, RIG_MODEL_FT950))
                return -RIG_EINVAL;
            /* RX only, on FT-2000/FT-9000 */
            which_ant = '5';
            break;
        default:
            return -RIG_EINVAL;
    }

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c%c", command, main_sub_vfo, which_ant, cat_term);
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));

    return err;
}


int newcat_get_ant(RIG * rig, vfo_t vfo, ant_t * ant)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char c;
    char command[] = "AN";
    char main_sub_vfo = '0';
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, command))
        return -RIG_ENAVAIL;

    /* Set Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    if (newcat_is_rig(rig, RIG_MODEL_FT9000))
        main_sub_vfo = (RIG_VFO_B == vfo) ? '1' : '0';

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", command, main_sub_vfo, cat_term);
    /* Get ANT */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, ANT value = %c\n", __func__, err, priv->ret_data, priv->ret_data[3]);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get ANT\n");
        return RIG_OK;
    }

    c = priv->ret_data[3];
    switch (c) {
        case '1':
            *ant = RIG_ANT_1;
            break;
        case '2' :
            *ant = RIG_ANT_2;
            break;
        case '3' :
            *ant = RIG_ANT_3;
            break;
        case '4' :
            *ant = RIG_ANT_4;
            break;
        case '5' :
            *ant = RIG_ANT_5;
            break;
        default:
            return -RIG_EPROTO;
    }

    return RIG_OK;
}


int newcat_set_level(RIG * rig, vfo_t vfo, setting_t level, value_t val)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    int i;
    int scale;
    int fpf;
    char main_sub_vfo = '0';

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;

    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    /* Set Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    /* Start with both but mostly FT9000 */
    if (newcat_is_rig(rig, RIG_MODEL_FT9000) || newcat_is_rig(rig, RIG_MODEL_FT2000))
        main_sub_vfo = (RIG_VFO_B == vfo) ? '1' : '0';

    switch (level) {
        case RIG_LEVEL_RFPOWER:
            if (!newcat_valid_command(rig, "PC"))
                return -RIG_ENAVAIL;
            scale = (newcat_is_rig(rig, RIG_MODEL_FT950)) ? 100 : 255;
            fpf = newcat_scale_float(scale, val.f);
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "PC%03d%c", fpf, cat_term);
            break;
        case RIG_LEVEL_AF:
            if (!newcat_valid_command(rig, "AG"))
                return -RIG_ENAVAIL;
            fpf = newcat_scale_float(255, val.f);
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "AG%c%03d%c", main_sub_vfo, fpf, cat_term);
            break;
        case RIG_LEVEL_AGC:
            if (!newcat_valid_command(rig, "GT"))
                return -RIG_ENAVAIL;
            switch (val.i) {
                case RIG_AGC_OFF: strcpy(priv->cmd_str, "GT00;"); break;
                case RIG_AGC_FAST: strcpy(priv->cmd_str, "GT01;"); break;
                case RIG_AGC_MEDIUM: strcpy(priv->cmd_str, "GT02;"); break;
                case RIG_AGC_SLOW: strcpy(priv->cmd_str, "GT03;"); break;
                case RIG_AGC_AUTO: strcpy(priv->cmd_str, "GT04;"); break;
                default: return -RIG_EINVAL;
            }
            priv->cmd_str[2] = main_sub_vfo;	
            break;
        case RIG_LEVEL_IF:
            if (!newcat_valid_command(rig, "IS"))
                return -RIG_ENAVAIL;
            if (abs(val.i) > rig->caps->max_ifshift) {
                if (val.i > 0)
                    val.i = rig->caps->max_ifshift;
                else
                    val.i = rig->caps->max_ifshift * -1;
            }
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "IS0%+.4d%c", val.i, cat_term);	/* problem with %+04d */
            if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_LEVEL_CWPITCH:
            if (!newcat_valid_command(rig, "KP"))
                return -RIG_ENAVAIL;
            if (val.i < 300)
                i = 300;
            else if (val.i > 1050)
                i = 1050;
            else
                i = val.i;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "KP%02d%c", 2*((i+50-300)/100), cat_term);
            break;
        case RIG_LEVEL_KEYSPD:
            if (!newcat_valid_command(rig, "KS"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "KS%03d%c", val.i, cat_term);
            break;
        case RIG_LEVEL_MICGAIN:
            if (!newcat_valid_command(rig, "MG"))
                return -RIG_ENAVAIL;
            fpf = newcat_scale_float(255, val.f);
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "MG%03d%c", fpf, cat_term);
            break;
        case RIG_LEVEL_METER:
            if (!newcat_valid_command(rig, "MS"))
                return -RIG_ENAVAIL;
            switch (val.i) {
                case RIG_METER_ALC: strcpy(priv->cmd_str, "MS1;"); break;
                case RIG_METER_PO:
                                    if (newcat_is_rig(rig, RIG_MODEL_FT950))
                                        return RIG_OK;
                                    else
                                        strcpy(priv->cmd_str, "MS2;");
                                    break;
                case RIG_METER_SWR:  strcpy(priv->cmd_str, "MS3;"); break;
                case RIG_METER_COMP: strcpy(priv->cmd_str, "MS0;"); break;
                case RIG_METER_IC:   strcpy(priv->cmd_str, "MS4;"); break;
                case RIG_METER_VDD:  strcpy(priv->cmd_str, "MS5;"); break;
                default: return -RIG_EINVAL;
            }
            break;
        case RIG_LEVEL_PREAMP:
            if (!newcat_valid_command(rig, "PA"))
                return -RIG_ENAVAIL;
            if (val.i == 0) {
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "PA00%c", cat_term);
                if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                    priv->cmd_str[2] = main_sub_vfo;
                break;
            }
            priv->cmd_str[0] = '\0';
            for (i=0; state->preamp[i] != RIG_DBLST_END; i++)
                if (state->preamp[i] == val.i) {
                    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "PA0%d%c", i+1, cat_term);
                    break;
                }
            if (strlen(priv->cmd_str) != 0) {
                if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                    priv->cmd_str[2] = main_sub_vfo;
                break;
            }

            return -RIG_EINVAL;
        case RIG_LEVEL_ATT:
            if (!newcat_valid_command(rig, "RA"))
                return -RIG_ENAVAIL;
            if (val.i == 0) {
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RA00%c", cat_term);
                if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                    priv->cmd_str[2] = main_sub_vfo;
                break;
            }
            priv->cmd_str[0] = '\0';
            for (i=0; state->attenuator[i] != RIG_DBLST_END; i++)
                if (state->attenuator[i] == val.i) {
                    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RA0%d%c", i+1, cat_term);
                    break;  /* for loop */
                }
            if (strlen(priv->cmd_str) != 0) {
                if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                    priv->cmd_str[2] = main_sub_vfo;
                break;
            }

            return -RIG_EINVAL;
        case RIG_LEVEL_RF:
            if (!newcat_valid_command(rig, "RG"))
                return -RIG_ENAVAIL;
            fpf = newcat_scale_float(255, val.f);
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RG%c%03d%c", main_sub_vfo, fpf, cat_term);
            break;
        case RIG_LEVEL_NR:
            if (!newcat_valid_command(rig, "RL"))
                return -RIG_ENAVAIL;
            if (newcat_is_rig(rig, RIG_MODEL_FT450)) {
                fpf = newcat_scale_float(11, val.f);
                if (fpf < 1)
                    fpf = 1;
                if (fpf > 11)
                    fpf = 11;
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RL0%02d%c", fpf, cat_term);
            } else {
                fpf = newcat_scale_float(15, val.f);
                if (fpf < 1)
                    fpf = 1;
                if (fpf > 15)
                    fpf = 15;
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RL0%02d%c", fpf, cat_term);
                if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                    priv->cmd_str[2] = main_sub_vfo;
            }
            break;
        case RIG_LEVEL_COMP:
            if (!newcat_valid_command(rig, "PL"))
                return -RIG_ENAVAIL;
            scale = (newcat_is_rig(rig, RIG_MODEL_FT950)) ? 100 : 255;
            fpf = newcat_scale_float(scale, val.f);
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "PL%03d%c", fpf, cat_term);
            break;
        case RIG_LEVEL_BKINDL:
            /* Standard: word "PARIS" == 50 Unit Intervals, UIs */
            /* 1 dot == 2 UIs */
            /* tenth_dots-per-second -to- milliseconds */
            if (!newcat_valid_command(rig, "SD"))
                return -RIG_ENAVAIL;
            if (val.i < 1)
                val.i = 1;
            val.i = 5000 / val.i;
            if (newcat_is_rig(rig, RIG_MODEL_FT950) || newcat_is_rig(rig, RIG_MODEL_FT450)) {
                if (val.i < 30)
                    val.i = 30;
                if (val.i > 3000)
                    val.i = 3000;
            } else if (newcat_is_rig(rig, RIG_MODEL_FT2000) || newcat_is_rig(rig, RIG_MODEL_FT9000)) {
                if (val.i < 1)
                    val.i = 1;
                if (val.i > 5000)
                    val.i = 5000;
            }
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "SD%04d%c", val.i, cat_term);
            break;
        case RIG_LEVEL_SQL:
            if (!newcat_valid_command(rig, "SQ"))
                return -RIG_ENAVAIL;
            fpf = newcat_scale_float(255, val.f);
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "SQ%c%03d%c", main_sub_vfo, fpf, cat_term);
            break;
        case RIG_LEVEL_VOX:
            if (!newcat_valid_command(rig, "VD"))
                return -RIG_ENAVAIL;
            /* VOX delay, arg int (tenth of seconds), 100ms UI */
            val.i = val.i * 100;
            if (newcat_is_rig(rig, RIG_MODEL_FT950) || newcat_is_rig(rig, RIG_MODEL_FT450)) {
                if (val.i < 100)         /* min is 30ms but spec is 100ms Unit Intervals */
                    val.i = 30;
                if (val.i > 3000)
                    val.i =3000;
            } else if (newcat_is_rig(rig, RIG_MODEL_FT2000) || newcat_is_rig(rig, RIG_MODEL_FT9000)) {
                if (val.i < 0)
                    val.i = 0;
                if (val.i > 5000)
                    val.i = 5000;
            }
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "VD%04d%c", val.i, cat_term);
            break;
        case RIG_LEVEL_VOXGAIN:
            if (!newcat_valid_command(rig, "VG"))
                return -RIG_ENAVAIL;
            scale = (newcat_is_rig(rig, RIG_MODEL_FT950)) ? 100 : 255;
            fpf = newcat_scale_float(scale, val.f);
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "VG%03d%c", fpf, cat_term);
            break;
        case RIG_LEVEL_ANTIVOX:
            if (newcat_is_rig(rig, RIG_MODEL_FT950)) {
                fpf = newcat_scale_float(100, val.f);
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "EX117%03d%c", fpf, cat_term);
            } else
                return -RIG_EINVAL;
            break;
        case RIG_LEVEL_NOTCHF:
            if (!newcat_valid_command(rig, "BP"))
                return -RIG_ENAVAIL;
            val.i = val.i / 10;
            if (val.i < 1)      /* fix lower bounds limit */
                val.i = 1;
            if (newcat_is_rig(rig, RIG_MODEL_FT950)) {
                if (val.i > 300)
                    val.i = 300;
            } else {
                if (val.i > 400)
                    val.i = 400;
            }
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "BP01%03d%c", val.i, cat_term);
            if (newcat_is_rig(rig, RIG_MODEL_FT9000)) /* The old CAT Man. shows VFO */
                priv->cmd_str[2] = main_sub_vfo;
            break;
        default:
            return -RIG_EINVAL;
    }

    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));

    return err;
}


int newcat_get_level(RIG * rig, vfo_t vfo, setting_t level, value_t * val)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    int ret_data_len;
    char *retlvl;
    float scale;
    char main_sub_vfo = '0';

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;

    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    /* Set Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    if (newcat_is_rig(rig, RIG_MODEL_FT9000) || newcat_is_rig(rig, RIG_MODEL_FT2000))
        main_sub_vfo = (RIG_VFO_B == vfo) ? '1' : '0';

    switch (level) {
        case RIG_LEVEL_RFPOWER:
            if (!newcat_valid_command(rig, "PC"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "PC%c", cat_term);
            break;
        case RIG_LEVEL_PREAMP:
            if (!newcat_valid_command(rig, "PA"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "PA0%c", cat_term);
            if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_LEVEL_AF:
            if (!newcat_valid_command(rig, "AG"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "AG%c%c", main_sub_vfo, cat_term);
            break;
        case RIG_LEVEL_AGC:
            if (!newcat_valid_command(rig, "GT"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "GT%c%c", main_sub_vfo, cat_term);
            break;
        case RIG_LEVEL_IF:
            if (!newcat_valid_command(rig, "IS"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "IS0%c", cat_term);
            if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_LEVEL_CWPITCH:
            if (!newcat_valid_command(rig, "KP"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "KP%c", cat_term);
            break;
        case RIG_LEVEL_KEYSPD:
            if (!newcat_valid_command(rig, "KS"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "KS%c", cat_term);
            break;
        case RIG_LEVEL_MICGAIN:
            if (!newcat_valid_command(rig, "MG"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "MG%c", cat_term);
            break;
        case RIG_LEVEL_METER:
            if (!newcat_valid_command(rig, "MS"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "MS%c", cat_term);
            break;
        case RIG_LEVEL_ATT:
            if (!newcat_valid_command(rig, "RA"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RA0%c", cat_term);
            if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_LEVEL_RF:
            if (!newcat_valid_command(rig, "RG"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RG%c%c", main_sub_vfo, cat_term);
            break;
        case RIG_LEVEL_COMP:
            if (!newcat_valid_command(rig, "PL"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "PL%c", cat_term);
            break;
        case RIG_LEVEL_NR:
            if (!newcat_valid_command(rig, "RL"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RL0%c", cat_term);
            if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_LEVEL_BKINDL:
            if (!newcat_valid_command(rig, "SD"))
                return -RIG_ENAVAIL;
            /* should be tenth of dots */
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "SD%c", cat_term);
            break;
        case RIG_LEVEL_SQL:
            if (!newcat_valid_command(rig, "SQ"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "SQ%c%c", main_sub_vfo, cat_term);
            break;
        case RIG_LEVEL_VOX:
            /* VOX delay, arg int (tenth of seconds) */
            if (!newcat_valid_command(rig, "VD"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "VD%c", cat_term);
            break;
        case RIG_LEVEL_VOXGAIN:
            if (!newcat_valid_command(rig, "VG"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "VG%c", cat_term);
            break;
            /*
             * Read only levels
             */
        case RIG_LEVEL_RAWSTR:
            if (!newcat_valid_command(rig, "SM"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "SM%c%c", main_sub_vfo, cat_term);
            break;
        case RIG_LEVEL_SWR:
            if (!newcat_valid_command(rig, "RM"))
                return -RIG_ENAVAIL;
            if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RM09%c", cat_term);
            else
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RM6%c", cat_term);
            break;
        case RIG_LEVEL_ALC:
            if (!newcat_valid_command(rig, "RM"))
                return -RIG_ENAVAIL;
            if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RM07%c", cat_term);
            else 
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "RM4%c", cat_term);
            break;
        case RIG_LEVEL_ANTIVOX:
            if (newcat_is_rig(rig, RIG_MODEL_FT950)) {
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "EX117%c", cat_term);
            } else
                return -RIG_EINVAL;
            break;
        case RIG_LEVEL_NOTCHF:
            if (!newcat_valid_command(rig, "BP"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "BP01%c", cat_term);
            if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                priv->cmd_str[2] = main_sub_vfo;
            break;
        default:
            return -RIG_EINVAL;
    }

    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data),
            &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s\n",
            __func__, err, priv->ret_data);

    ret_data_len = strlen(priv->ret_data);

    if (ret_data_len <= strlen(priv->cmd_str) ||
            priv->ret_data[ret_data_len-1] != cat_term)
        return -RIG_EPROTO;

    /* skip command */
    retlvl = priv->ret_data + strlen(priv->cmd_str)-1;
    /* chop term */
    priv->ret_data[ret_data_len-1] = '\0';

    switch (level) {
        case RIG_LEVEL_RFPOWER:
        case RIG_LEVEL_VOXGAIN:
        case RIG_LEVEL_COMP:
        case RIG_LEVEL_ANTIVOX:
            scale = (newcat_is_rig(rig, RIG_MODEL_FT950)) ? 100. : 255.;
            val->f = (float)atoi(retlvl)/scale;
            break;
        case RIG_LEVEL_AF:
        case RIG_LEVEL_MICGAIN:
        case RIG_LEVEL_RF:
        case RIG_LEVEL_SQL:
        case RIG_LEVEL_SWR:
        case RIG_LEVEL_ALC:
            val->f = (float)atoi(retlvl)/255.;
            break;
        case RIG_LEVEL_BKINDL:
            val->i = atoi(retlvl);      /* milliseconds */
            if (val->i < 1)
                val->i = 1;
            val->i = 5000 / val->i;   /* ms -to- tenth_dots-per-second */
            if (val->i < 1)
                val->i = 1;
            break;
        case RIG_LEVEL_RAWSTR:
        case RIG_LEVEL_KEYSPD:
        case RIG_LEVEL_IF:
            val->i = atoi(retlvl);
            break;
        case RIG_LEVEL_NR:
            /*  ratio 0 - 1.0 */
            if (newcat_is_rig(rig, RIG_MODEL_FT450))
                val->f = (float) (atoi(retlvl) / 11. );
            else
                val->f = (float) (atoi(retlvl) / 15. );
            break;
        case RIG_LEVEL_VOX:
            /* VOX delay, arg int (tenth of seconds), 100ms intervals */
            val->i = atoi(retlvl)/100;
            break;
        case RIG_LEVEL_PREAMP:
            if (retlvl[0] < '0' || retlvl[0] > '9')
                return -RIG_EPROTO;
            val->i = (retlvl[0] == '0') ? 0 : state->preamp[retlvl[0]-'1'];
            break;
        case RIG_LEVEL_ATT:
            if (retlvl[0] < '0' || retlvl[0] > '9')
                return -RIG_EPROTO;
            val->i = (retlvl[0] == '0') ? 0 : state->attenuator[retlvl[0]-'1'];
            break;
        case RIG_LEVEL_AGC:
            switch (retlvl[0]) {
                case '0': val->i = RIG_AGC_OFF; break;
                case '1': val->i = RIG_AGC_FAST; break;
                case '2': val->i = RIG_AGC_MEDIUM; break;
                case '3': val->i = RIG_AGC_SLOW; break;
                case '4': 
                case '5':
                case '6':
                          val->i = RIG_AGC_AUTO; break;
                default: return -RIG_EPROTO;
            }
            break;
        case RIG_LEVEL_CWPITCH:
            val->i = (atoi(retlvl)/2)*100+300;
            break;
        case RIG_LEVEL_METER:
            switch (retlvl[0]) {
                case '0': val->i = RIG_METER_COMP; break;
                case '1': val->i = RIG_METER_ALC; break;
                case '2': val->i = RIG_METER_PO; break;
                case '3': val->i = RIG_METER_SWR; break;
                case '4': val->i = RIG_METER_IC; break;     /* ID CURRENT */
                case '5': val->i = RIG_METER_VDD; break;    /* Final Amp Voltage */
                default: return -RIG_EPROTO;
            }
            break;
        case RIG_LEVEL_NOTCHF:
            val->i = atoi(retlvl) * 10;
            break;
        default:
            return -RIG_EINVAL;
    }

    return RIG_OK;
}


int newcat_set_func(RIG * rig, vfo_t vfo, setting_t func, int status)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char main_sub_vfo = '0';

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;

    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    /* Set Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    if (newcat_is_rig(rig, RIG_MODEL_FT9000) || newcat_is_rig(rig, RIG_MODEL_FT2000))
        main_sub_vfo = (RIG_VFO_B == vfo) ? '1' : '0';

    switch (func) {
        case RIG_FUNC_ANF:
            if (!newcat_valid_command(rig, "BC"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "BC0%d%c", status ? 1 : 0, cat_term);
            if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_FUNC_MN:
            if (!newcat_valid_command(rig, "BP"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "BP00%03d%c", status ? 1 : 0, cat_term);
            if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_FUNC_FBKIN:
            if (!newcat_valid_command(rig, "BI"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "BI%d%c", status ? 1 : 0, cat_term);
            break;
        case RIG_FUNC_TONE:
            if (!newcat_valid_command(rig, "CT"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "CT0%d%c", status ? 2 : 0, cat_term);
            priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_FUNC_TSQL:
            if (!newcat_valid_command(rig, "CT"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "CT0%d%c", status ? 1 : 0, cat_term);
            priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_FUNC_LOCK:
            if (!newcat_valid_command(rig, "LK"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "LK%d%c", status ? 1 : 0, cat_term);
            break;
        case RIG_FUNC_MON:
            if (!newcat_valid_command(rig, "ML"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "ML0%03d%c", status ? 1 : 0, cat_term);
            break;
        case RIG_FUNC_NB:
            if (!newcat_valid_command(rig, "NB"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "NB0%d%c", status ? 1 : 0, cat_term);
            priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_FUNC_NR:
            if (!newcat_valid_command(rig, "NR"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "NR0%d%c", status ? 1 : 0, cat_term);
            priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_FUNC_COMP:
            if (!newcat_valid_command(rig, "PR"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "PR%d%c", status ? 1 : 0, cat_term);
            break;
        case RIG_FUNC_VOX:
            if (!newcat_valid_command(rig, "VX"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "VX%d%c", status ? 1 : 0, cat_term);
            break;
        default:
            return -RIG_EINVAL;
    }

    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));

    return err;
}


int newcat_get_func(RIG * rig, vfo_t vfo, setting_t func, int *status)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    int ret_data_len;
    char *retfunc;
    char main_sub_vfo = '0';

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;

    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    switch (func) {
        case RIG_FUNC_ANF:
            if (!newcat_valid_command(rig, "BC"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "BC0%c", cat_term);
            if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_FUNC_MN:
            if (!newcat_valid_command(rig, "BP"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "BP00%c", cat_term);
            if (newcat_is_rig(rig, RIG_MODEL_FT9000))
                priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_FUNC_FBKIN:
            if (!newcat_valid_command(rig, "BI"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "BI%c", cat_term);
            break;
        case RIG_FUNC_TONE:
            if (!newcat_valid_command(rig, "CT"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "CT0%c", cat_term);
            priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_FUNC_TSQL:
            if (!newcat_valid_command(rig, "CT"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "CT0%c", cat_term);
            priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_FUNC_LOCK:
            if (!newcat_valid_command(rig, "LK"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "LK%c", cat_term);
            break;
        case RIG_FUNC_MON:
            if (!newcat_valid_command(rig, "ML"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "ML0%c", cat_term);
            break;
        case RIG_FUNC_NB:
            if (!newcat_valid_command(rig, "NB"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "NB0%c", cat_term);
            priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_FUNC_NR:
            if (!newcat_valid_command(rig, "NR"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "NR0%c", cat_term);
            priv->cmd_str[2] = main_sub_vfo;
            break;
        case RIG_FUNC_COMP:
            if (!newcat_valid_command(rig, "PR"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "PR%c", cat_term);
            break;
        case RIG_FUNC_VOX:
            if (!newcat_valid_command(rig, "VX"))
                return -RIG_ENAVAIL;
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "VX%c", cat_term);
            break;
        default:
            return -RIG_EINVAL;
    }

    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data),
            &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s\n",
            __func__, err, priv->ret_data);

    ret_data_len = strlen(priv->ret_data);
    if (ret_data_len <= strlen(priv->cmd_str) ||
            priv->ret_data[ret_data_len-1] != cat_term)
        return -RIG_EPROTO;

    /* skip command */
    retfunc = priv->ret_data + strlen(priv->cmd_str)-1;
    /* chop term */
    priv->ret_data[ret_data_len-1] = '\0';

    switch (func) {
        case RIG_FUNC_MN:
            *status = (retfunc[2] == '0') ? 0 : 1;
            break;
        case RIG_FUNC_ANF:
        case RIG_FUNC_FBKIN:
        case RIG_FUNC_LOCK:
        case RIG_FUNC_MON:
        case RIG_FUNC_NB:
        case RIG_FUNC_NR:
        case RIG_FUNC_COMP:
        case RIG_FUNC_VOX:
            *status = (retfunc[0] == '0') ? 0 : 1;
            break;

        case RIG_FUNC_TONE:
            *status = (retfunc[0] == '2') ? 1 : 0;
            break;
        case RIG_FUNC_TSQL:
            *status = (retfunc[0] == '1') ? 1 : 0;
            break;
        default:
            return -RIG_EINVAL;
    }

    return RIG_OK;
}


int newcat_set_parm(RIG * rig, setting_t parm, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_get_parm(RIG * rig, setting_t parm, value_t * val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_get_ext_level(RIG *rig, vfo_t vfo, token_t token, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_ext_parm(RIG *rig, token_t token, value_t val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_get_ext_parm(RIG *rig, token_t token, value_t *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_conf(RIG * rig, token_t token, const char *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_get_conf(RIG * rig, token_t token, char *val)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_send_dtmf(RIG * rig, vfo_t vfo, const char *digits)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_recv_dtmf(RIG * rig, vfo_t vfo, char *digits, int *length)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_send_morse(RIG * rig, vfo_t vfo, const char *msg)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_bank(RIG * rig, vfo_t vfo, int bank)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_mem(RIG * rig, vfo_t vfo, int ch)
{
    int err;

    err = newcat_set_any_mem(rig, vfo, ch);

    return err;
}


int newcat_get_mem(RIG * rig, vfo_t vfo, int *ch)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "MC"))
        return -RIG_ENAVAIL;

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "MC%c", cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Get Memory Channel Number */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data),
            &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n",
                __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s\n",
            __func__, err, priv->ret_data);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get MEM\n");
        /* Invalid channel should never see this from radio  */
        return RIG_OK;
    }

    *ch = atoi(priv->ret_data + 2);

    return RIG_OK;
}

int newcat_vfo_op(RIG * rig, vfo_t vfo, vfo_op_t op)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char main_sub_vfo = '0';

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return -RIG_EINVAL;


    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    /* Set Main or SUB vfo */
    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    if (newcat_is_rig(rig, RIG_MODEL_FT9000) || newcat_is_rig(rig, RIG_MODEL_FT2000))
        main_sub_vfo = (RIG_VFO_B == vfo) ? '1' : '0';

    switch (op) {
        case RIG_OP_TUNE:
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "AC002%c", cat_term);
            break;
        case RIG_OP_CPY:
            if (newcat_is_rig(rig, RIG_MODEL_FT450))
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "VV%c", cat_term);
            else
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "AB%c", cat_term);
            break;
        case RIG_OP_XCHG:
        case RIG_OP_TOGGLE:
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "SV%c", cat_term);
            break;
        case RIG_OP_UP:
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "UP%c", cat_term);
            break;
        case RIG_OP_DOWN:
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "DN%c", cat_term);
            break;
        case RIG_OP_BAND_UP:
            if (main_sub_vfo == 1)
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "BU1%c", cat_term);
            else
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "BU0%c", cat_term);
            break;
        case RIG_OP_BAND_DOWN:
            if (main_sub_vfo == 1)
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "BD1%c", cat_term);
            else
                snprintf(priv->cmd_str, sizeof(priv->cmd_str), "BD0%c", cat_term);
            break;
        case RIG_OP_FROM_VFO:
            /* VFOA ! */
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "AM%c", cat_term);
            break;
        case RIG_OP_TO_VFO:
            /* VFOA ! */
            snprintf(priv->cmd_str, sizeof(priv->cmd_str), "MA%c", cat_term);
            break;
        default:
            return -RIG_EINVAL;
    }

    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));

    return err;
}


int newcat_scan(RIG * rig, vfo_t vfo, scan_t scan, int ch)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_trn(RIG * rig, int trn)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char c;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "AI"))
        return -RIG_ENAVAIL;

    if (trn  == RIG_TRN_OFF) 
        c = '0';
    else 
        c = '1';

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "AI%c%c", c, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));

    return err;
}


int newcat_get_trn(RIG * rig, int *trn)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char c;
    char command[] = "AI";
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, command))
        return -RIG_ENAVAIL;

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", command, cat_term);
    /* Get Auto Information */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, TRN value = %c\n", __func__, err, priv->ret_data, priv->ret_data[2]);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get TRN\n");
        return RIG_OK;
    }

    c = priv->ret_data[2];
    if (c == '0')
        *trn = RIG_TRN_OFF;
    else
        *trn = RIG_TRN_RIG;

    return RIG_OK;
}


int newcat_decode_event(RIG * rig)
{
    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    return -RIG_ENAVAIL;
}


int newcat_set_channel(RIG * rig, const channel_t * chan)
{
    int err;

    err = newcat_set_any_channel(rig, chan);

    return err;
}


int newcat_get_channel(RIG * rig, channel_t * chan)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    char *retval;
    char c, c2;
    int err, i;
    chan_t *chan_list;
    channel_cap_t *mem_caps = NULL;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "MR"))
        return -RIG_ENAVAIL;

    chan_list = rig->caps->chan_list;

    for (i=0; i<CHANLSTSIZ && !RIG_IS_CHAN_END(chan_list[i]); i++) {
        if (chan->channel_num >= chan_list[i].start &&
                chan->channel_num <= chan_list[i].end) {
            mem_caps = &chan_list[i].mem_caps;
            break;
        }
    }

    /* Out of Range */
    if (!mem_caps)
        return -RIG_ENAVAIL;

    rig_debug(RIG_DEBUG_TRACE, "sizeof(channel_t) = %d\n", sizeof(channel_t) );
    rig_debug(RIG_DEBUG_TRACE, "sizeof(priv->cmd_str) = %d\n", sizeof(priv->cmd_str) );

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "MR%03d%c", chan->channel_num, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);


    /* Get Memory Channel */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, ret_data length = %d\n", __func__, err, priv->ret_data, strlen(priv->ret_data));

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get_channel, Invalid channel...\n");
        /* Invalid channel, has not been set up, make sure freq is 0 to indicate empty channel */
        chan->freq = 0.0; 
        return RIG_OK;
    }

    /* ret_data string to channel_t struct :: this will destroy ret_data */

    /* rptr_shift P10 ************************ */
    retval = priv->ret_data + 25;
    switch (*retval) {
        case '0': chan->rptr_shift = RIG_RPT_SHIFT_NONE;  break;
        case '1': chan->rptr_shift = RIG_RPT_SHIFT_PLUS;  break;
        case '2': chan->rptr_shift = RIG_RPT_SHIFT_MINUS; break;
        default:  chan->rptr_shift = RIG_RPT_SHIFT_NONE;
    }
    *retval = '\0';

    /* CTCSS Encoding P8 ********************* */
    retval = priv->ret_data + 22;
    c = *retval;

    /* CTCSS Tone P9 ************************* */
    chan->ctcss_tone = 0;
    chan->ctcss_sql  = 0;
    retval = priv->ret_data + 23;
    i = atoi(retval);

    if (c == '1')
        chan->ctcss_sql = rig->caps->ctcss_list[i];
    else if (c == '2')
        chan->ctcss_tone = rig->caps->ctcss_list[i];

    /* vfo, mem, P7 ************************** */
    retval = priv->ret_data + 21;

    if (*retval == '1')
        chan->vfo = RIG_VFO_MEM;
    else 
        chan->vfo = RIG_VFO_CURR;

    /* MODE P6 ******************************* */
    chan->width = 0;

    retval = priv->ret_data + 20;
    switch (*retval) {
        case '1': chan->mode = RIG_MODE_LSB;    break;
        case '2': chan->mode = RIG_MODE_USB;    break;
        case '3': chan->mode = RIG_MODE_CW;     break;
        case '4': chan->mode = RIG_MODE_FM;     break;
        case '5': chan->mode = RIG_MODE_AM;     break;
        case '6': chan->mode = RIG_MODE_RTTY;   break;
        case '7': chan->mode = RIG_MODE_CWR;    break;
        case '8': chan->mode = RIG_MODE_PKTLSB; break;
        case '9': chan->mode = RIG_MODE_RTTYR;  break;
        case 'A': chan->mode = RIG_MODE_PKTFM;  break;
        case 'B': chan->mode = RIG_MODE_FM;     break;
        case 'C': chan->mode = RIG_MODE_PKTUSB; break;
        case 'D': chan->mode = RIG_MODE_AM;     break;
        default:  chan->mode = RIG_MODE_LSB;
    }

    /* Clarifier TX P5 *********************** */
    retval = priv->ret_data + 19; 
    c2 = *retval;

    /* Clarifier RX P4 *********************** */
    retval = priv->ret_data + 18;
    c = *retval;
    *retval = '\0';

    /* Clarifier Offset P3 ******************* */
    chan->rit = 0;
    chan->xit = 0;
    retval = priv->ret_data + 13;
    if (c == '1')
        chan->rit = atoi(retval);
    else if (c2 == '1')
        chan->xit = atoi(retval);
    *retval = '\0';

    /* Frequency P2 ************************** */
    retval = priv->ret_data + 5;
    chan->freq = atof(retval);

    return RIG_OK;
}


const char *newcat_get_info(RIG * rig)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    static char idbuf[8];
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig)
        return NULL;

    /* Build the command string */
    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "ID;");

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Get Identification Channel */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return NULL;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data),
            &cat_term, sizeof(cat_term));
    if (err < 0)
        return NULL;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n",
                __func__, priv->ret_data);

        return NULL;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s\n",
            __func__, err, priv->ret_data);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get INFO\n");
        return NULL;
    }

    priv->ret_data[6] = '\0';
    strcpy(idbuf, priv->ret_data);

    return idbuf;
}

// const char *clone_combo_set;	/*!< String describing key combination to enter load cloning mode */
// const char *clone_combo_get;	/*!< String describing key combination to enter save cloning mode */


/*
 * newcat_valid_command
 *
 * Determine whether or not the command is valid for the specified
 * rig.  This function should be called before sending the command
 * to the rig to make it easier to differentiate invalid and illegal
 * commands (for a rig).
 */

ncboolean newcat_valid_command(RIG *rig, char *command) {
    const struct rig_caps *caps;
    ncboolean is_ft450;
    ncboolean is_ft950;
    ncboolean is_ft2000;
    ncboolean is_ft9000;
    int search_high;
    int search_index;
    int search_low;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!rig) {
        rig_debug(RIG_DEBUG_ERR, "%s: Rig argument is invalid\n", __func__);
        return FALSE;
    }

    caps = rig->caps;
    if (!caps) {
        rig_debug(RIG_DEBUG_ERR, "%s: Rig capabilities not valid\n", __func__);
        return FALSE;
    }

    /*
     * Determine the type of rig from the model number.  Note it is
     * possible for several model variants to exist; i.e., all the
     * FT-9000 variants.
     */

    is_ft450 = newcat_is_rig(rig, RIG_MODEL_FT450);
    is_ft950 = newcat_is_rig(rig, RIG_MODEL_FT950);
    is_ft2000 = newcat_is_rig(rig, RIG_MODEL_FT2000);
    is_ft9000 = newcat_is_rig(rig, RIG_MODEL_FT9000);


    if (!is_ft450 && !is_ft950 && !is_ft2000 && !is_ft9000) {
        rig_debug(RIG_DEBUG_ERR, "%s: '%s' is unknown\n",
                __func__, caps->model_name);
        return FALSE;
    }

    /*
     * Make sure the command is known, and then check to make sure
     * is it valud for the rig.
     */

    search_low = 0;
    search_high = valid_commands_count;
    while (search_low <= search_high) {
        int search_test;

        search_index = (search_low + search_high) / 2;
        search_test = strcmp (valid_commands[search_index].command, command);
        if (search_test > 0)
            search_high = search_index - 1;
        else if (search_test < 0)
            search_low = search_index + 1;
        else {
            /*
             * The command is valid.  Now make sure it is supported by the rig.
             */
            if (is_ft450 && valid_commands[search_index].ft450)
                return TRUE;
            else if (is_ft950 && valid_commands[search_index].ft950)
                return TRUE;
            else if (is_ft2000 && valid_commands[search_index].ft2000)
                return TRUE;
            else if (is_ft9000 && valid_commands[search_index].ft9000)
                return TRUE;
            else {
                rig_debug(RIG_DEBUG_TRACE, "%s: '%s' command '%s' not supported\n",
                        __func__, caps->model_name, command);
                return FALSE;
            }
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: '%s' command '%s' not valid\n",
            __func__, caps->model_name, command);
    return FALSE;
}


ncboolean newcat_is_rig(RIG * rig, rig_model_t model) {
    ncboolean is_rig;

    is_rig = (model == rig->caps->rig_model) ? TRUE : FALSE;

    return is_rig;
}


/*
 * newcat_set_tx_vfo does not set priv->curr_vfo
 */
int newcat_set_tx_vfo(RIG * rig, vfo_t tx_vfo) {
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char p1;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "FT"))
        return -RIG_ENAVAIL;

    err = newcat_set_vfo_from_alias(rig, &tx_vfo);
    if (err < 0)
        return err;

    switch (tx_vfo) {
        case RIG_VFO_A:
            p1 = '0';
            break;
        case RIG_VFO_B:
            p1 = '1';
            break;
        case RIG_VFO_MEM:
            /* VFO A */
            if (priv->current_mem == NC_MEM_CHANNEL_NONE)
                return RIG_OK;
            else    /* Memory Channel mode */
                p1 = '0';
            break;
        default:
            return -RIG_EINVAL;
    }

    if (newcat_is_rig(rig, RIG_MODEL_FT950))
        p1 = p1 + 2;            /* FT950 non-Toggle */

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", "FT", p1, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

    /* Set TX VFO */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
        
    return err;
}


/*
 * newcat_get_tx_vfo does not set priv->curr_vfo
 */
int newcat_get_tx_vfo(RIG * rig, vfo_t * tx_vfo) {
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char c;
    vfo_t vfo_mode;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "FT"))
        return -RIG_ENAVAIL;

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", "FT", cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

    /* Get TX VFO */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, TX_VFO value = %c\n", __func__, err, priv->ret_data, priv->ret_data[2]);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get TX_VFO\n");
        return RIG_OK;
    }

    c = priv->ret_data[2];
    switch (c) {
        case '0':
            *tx_vfo = RIG_VFO_A;
            break;
        case '1' :
            *tx_vfo = RIG_VFO_B;
            break;
        default:
            return -RIG_EPROTO;
    }

    /* Check to see if RIG is in MEM mode */
    err = newcat_get_vfo_mode(rig, &vfo_mode);
    if (vfo_mode == RIG_VFO_MEM && *tx_vfo == RIG_VFO_A)
        *tx_vfo = RIG_VFO_MEM;

    rig_debug(RIG_DEBUG_TRACE, "%s: tx_vfo = 0x%02x\n", __func__, *tx_vfo);

    return RIG_OK;
}



/*
 * Uses "FR" command
 * Calls newcat_set_vfo() for FT450 rig that does not support "FR" 
 * newcat_set_vfo() Calls newcat_set_rx_vfo() for rigs that do not support "VS"
 */
int newcat_set_rx_vfo(RIG * rig, vfo_t rx_vfo) {
    struct newcat_priv_data *priv;
    struct rig_state *state;
    char p1;
    int err, mem;
    vfo_t vfo_mode;
    char command[] = "FR";
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, command)) {
        if (newcat_is_rig(rig, RIG_MODEL_FT450)) {    /* No "FR" command */
            err = newcat_set_vfo(rig, rx_vfo);
            return err;
        }   
        return -RIG_ENAVAIL;
    }

    err = newcat_set_vfo_from_alias(rig, &rx_vfo);
    if (err < 0)
        return err;

    switch (rx_vfo) {
        case RIG_VFO_A:
        case RIG_VFO_B:
            if (rx_vfo == RIG_VFO_B)
                p1 = '2';       /* Main Band VFO_A,   Sub Band VFO_B on some radios */
            else 
                p1 = '0';

            err = newcat_get_vfo_mode(rig, &vfo_mode);
            if (vfo_mode == RIG_VFO_MEM) {
                priv->current_mem = NC_MEM_CHANNEL_NONE;
                priv->current_vfo = RIG_VFO_A;
                err = newcat_vfomem_toggle(rig);
                return err;
            }
            break;
        case RIG_VFO_MEM:
            if (priv->current_mem == NC_MEM_CHANNEL_NONE) {
                /* Only works correctly for VFO A */
                if (priv->current_vfo == RIG_VFO_B)
                    return -RIG_ENTARGET;

                /* get current memory channel */
                err = newcat_get_mem(rig, rx_vfo, &mem);
                if (err != RIG_OK)
                    return err;

                /* turn on memory channel */
                err = newcat_set_mem(rig, rx_vfo, mem);
                if (err != RIG_OK)
                    return err;

                /* Set current_mem now */
                priv->current_mem = mem;
            }
            /* Set current_vfo now  */
            priv->current_vfo = rx_vfo; 
            return RIG_OK;
        default:
            return -RIG_EINVAL;
    }

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", command, p1, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

    /* Set RX VFO */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    priv->current_vfo = rx_vfo;      /* Track Main Band RX VFO */

    return RIG_OK;
}


/* 
 * Uses "FR" command
 * Calls newcat_get_vfo() for FT450 rig that does not support "FR" 
 * newcat_get_vfo() Calls newcat_get_rx_vfo() for rigs that do not support "VS"
 */
int newcat_get_rx_vfo(RIG * rig, vfo_t * rx_vfo) {
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char c;
    vfo_t vfo_mode;
    char command[] = "FR";
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, command)) {
        if (newcat_is_rig(rig, RIG_MODEL_FT450)) {    /* No "FR" command */
            err = newcat_get_vfo(rig, rx_vfo);
            if (err < 0)
                return err;

            return RIG_OK;
        }

        return -RIG_ENAVAIL;
    }

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", command, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

    /* Get RX VFO */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, RX_VFO value = %c\n", __func__, err, priv->ret_data, priv->ret_data[2]);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get RX_VFO\n");
        return RIG_OK;
    }

    c = priv->ret_data[2];
    switch (c) {
        case '0':                   /* Main Band VFO_A RX,   Sub Band VFO_B OFF */
        case '1':                   /* Main Band VFO_A Mute, Sub Band VFO_B OFF */
            *rx_vfo = RIG_VFO_A;
            break;
        case '2' :                  /* Main Band VFO_A RX,   Sub Band VFO_B RX */
        case '3' :                  /* Main Band VFO_A Mute, Sub Band VFO_B RX */
            *rx_vfo = RIG_VFO_A;
            break;
        case '4' :                  /* FT950 VFO_A OFF, FT950 VFO_B RX   */
        case '5' :                  /* FT950 VFO_A OFF, FT950 VFO_B Mute */
            *rx_vfo = RIG_VFO_B;
            break;
        default:
            return -RIG_EPROTO;
    }


    /* Check to see if RIG is in MEM mode */
    err = newcat_get_vfo_mode(rig, &vfo_mode);
    if (vfo_mode == RIG_VFO_MEM)
        *rx_vfo = RIG_VFO_MEM;

    priv->current_vfo = *rx_vfo;     /* set now */

    rig_debug(RIG_DEBUG_TRACE, "%s: rx_vfo = 0x%02x\n", __func__, *rx_vfo);

    return RIG_OK;
}


int newcat_set_vfo_from_alias(RIG * rig, vfo_t * vfo) {
    struct newcat_priv_data *priv;
    
    priv = (struct newcat_priv_data *)rig->state.priv;

    rig_debug(RIG_DEBUG_TRACE, "%s: alias vfo = 0x%02x\n", __func__, *vfo);

    switch (*vfo) {
        case RIG_VFO_A:
        case RIG_VFO_B:
        case RIG_VFO_MEM:
            /* passes through */
            break;
        case RIG_VFO_CURR:  /* RIG_VFO_RX == RIG_VFO_CURR */
        case RIG_VFO_VFO:
            *vfo = priv->current_vfo;
            break;
        case RIG_VFO_TX:
            /* set to another vfo for split or uplink */
            *vfo = (priv->current_vfo == RIG_VFO_B) ? RIG_VFO_A : RIG_VFO_B;
            break;
        case RIG_VFO_MAIN:
            *vfo = RIG_VFO_A;
            break;
        case RIG_VFO_SUB:
            *vfo = RIG_VFO_B;
            break;
        default:
            rig_debug(RIG_DEBUG_TRACE, "Unrecognized.  vfo= %d\n", *vfo);
            return -RIG_EINVAL;
    }

    return RIG_OK;
}

/*
 *	Found newcat_set_level() floating point math problem
 *	Using rigctl on FT950 I was trying to set RIG_LEVEL_COMP to 12
 *	I kept setting it to 11.  I wrote some test software and 
 *	found out that 0.12 * 100 = 11 with my setup.
 *  Compilier is gcc 4.2.4, CPU is AMD X2
 *  This works but Find a better way.
 *  The newcat_get_level() seems to work correctly.
 *  Terry KJ4EED
 *
 */
int newcat_scale_float(int scale, float fval) {
    float f;
    float fudge = 0.003;

    if ((fval + fudge) > 1.0)
        f = scale * fval;
    else 
        f = scale * (fval + fudge);

    return (int) f;
}


int newcat_set_narrow(RIG * rig, vfo_t vfo, ncboolean narrow)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char c;
    char main_sub_vfo = '0';
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "NA"))
        return -RIG_ENAVAIL;

    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    if (newcat_is_rig(rig, RIG_MODEL_FT9000) || newcat_is_rig(rig, RIG_MODEL_FT2000))
        main_sub_vfo = (RIG_VFO_B == vfo) ? '1' : '0';

    if (narrow == TRUE) 
        c = '1';
    else
        c = '0';

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "NA%c%c%c", main_sub_vfo, c, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));

    return err;
}


int newcat_get_narrow(RIG * rig, vfo_t vfo, ncboolean * narrow)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char c;
    char command[] = "NA";
    char main_sub_vfo = '0';
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, command))
        return -RIG_ENAVAIL;

    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    if (newcat_is_rig(rig, RIG_MODEL_FT9000) || newcat_is_rig(rig, RIG_MODEL_FT2000))
        main_sub_vfo = (RIG_VFO_B == vfo) ? '1' : '0';

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", command, main_sub_vfo, cat_term);
    /* Get NAR */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, NARROW value = %c\n", __func__, err, priv->ret_data, priv->ret_data[3]);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get NARROW\n");
        return RIG_OK;
    }

    c = priv->ret_data[3];
    if (c == '1')
        *narrow = TRUE;
    else
        *narrow = FALSE;

    return RIG_OK;
}


int newcat_set_rx_bandwidth(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t width)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char width_str[3];
    char main_sub_vfo = '0';
    char narrow = '0';
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "SH"))
        return -RIG_ENAVAIL;

    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    if (newcat_is_rig(rig, RIG_MODEL_FT9000) || newcat_is_rig(rig, RIG_MODEL_FT2000))
        main_sub_vfo = (RIG_VFO_B == vfo) ? '1' : '0';

    if (newcat_is_rig(rig, RIG_MODEL_FT950)) {
        switch (mode) {
            case RIG_MODE_PKTUSB:
            case RIG_MODE_PKTLSB:
            case RIG_MODE_RTTY:
            case RIG_MODE_RTTYR:
            case RIG_MODE_CW:
            case RIG_MODE_CWR:
                switch (width) {
                    case 1700: snprintf(width_str, sizeof(width_str), "11"); narrow = '0'; break;  /* normal */
                    case  500: snprintf(width_str, sizeof(width_str), "07"); narrow = '0'; break;  /* narrow */
                    case 2400: snprintf(width_str, sizeof(width_str), "13"); narrow = '0'; break;  /* wide */
                    case 2000: snprintf(width_str, sizeof(width_str), "12"); narrow = '0'; break;
                    case 1400: snprintf(width_str, sizeof(width_str), "10"); narrow = '0'; break;
                    case 1200: snprintf(width_str, sizeof(width_str), "09"); narrow = '0'; break;
                    case  800: snprintf(width_str, sizeof(width_str), "08"); narrow = '0'; break;
                    case  400: snprintf(width_str, sizeof(width_str), "06"); narrow = '1'; break;
                    case  300: snprintf(width_str, sizeof(width_str), "05"); narrow = '1'; break;
                    case  200: snprintf(width_str, sizeof(width_str), "04"); narrow = '1'; break;
                    case  100: snprintf(width_str, sizeof(width_str), "03"); narrow = '1'; break;
                    default: return -RIG_EINVAL;
                }
                break;
            case RIG_MODE_LSB:
            case RIG_MODE_USB:
                switch (width) {
                    case 2400: snprintf(width_str, sizeof(width_str), "13"); narrow = '0'; break;  /* normal */
                    case 1800: snprintf(width_str, sizeof(width_str), "09"); narrow = '0'; break;  /* narrow */
                    case 3000: snprintf(width_str, sizeof(width_str), "20"); narrow = '0'; break;  /* wide */
                    case 2900: snprintf(width_str, sizeof(width_str), "19"); narrow = '0'; break;
                    case 2800: snprintf(width_str, sizeof(width_str), "18"); narrow = '0'; break;
                    case 2700: snprintf(width_str, sizeof(width_str), "17"); narrow = '0'; break;
                    case 2600: snprintf(width_str, sizeof(width_str), "16"); narrow = '0'; break;
                    case 2500: snprintf(width_str, sizeof(width_str), "15"); narrow = '0'; break;
                    case 2450: snprintf(width_str, sizeof(width_str), "14"); narrow = '0'; break;
                    case 2250: snprintf(width_str, sizeof(width_str), "12"); narrow = '0'; break;
                    case 2100: snprintf(width_str, sizeof(width_str), "11"); narrow = '0'; break;
                    case 1950: snprintf(width_str, sizeof(width_str), "10"); narrow = '0'; break;
                    case 1650: snprintf(width_str, sizeof(width_str), "08"); narrow = '1'; break;
                    case 1500: snprintf(width_str, sizeof(width_str), "07"); narrow = '1'; break;
                    case 1350: snprintf(width_str, sizeof(width_str), "06"); narrow = '1'; break;
                    case 1100: snprintf(width_str, sizeof(width_str), "05"); narrow = '1'; break;
                    case  850: snprintf(width_str, sizeof(width_str), "04"); narrow = '1'; break;
                    case  600: snprintf(width_str, sizeof(width_str), "03"); narrow = '1'; break;
                    case  400: snprintf(width_str, sizeof(width_str), "02"); narrow = '1'; break;
                    case  200: snprintf(width_str, sizeof(width_str), "01"); narrow = '1';  break;
                    default: return -RIG_EINVAL;
                }
                break;
            case RIG_MODE_AM:
            case RIG_MODE_FM:
            case RIG_MODE_PKTFM:
                if (width < rig_passband_normal(rig, mode))
                    err = newcat_set_narrow(rig, vfo, TRUE);
                else
                    err = newcat_set_narrow(rig, vfo, FALSE);
                return err; 
            default:
                return -RIG_EINVAL;
        }   /* end switch(mode) */
    }   /* end if FT950 */
    else {
        /* FT450, FT2000, FT9000 */
        switch (mode) {
            case RIG_MODE_PKTUSB:
            case RIG_MODE_PKTLSB:
            case RIG_MODE_RTTY:
            case RIG_MODE_RTTYR:
            case RIG_MODE_CW:
            case RIG_MODE_CWR:
                switch (width) {
                    case 1800: snprintf(width_str, sizeof(width_str), "16"); narrow = '0'; break;  /* normal */
                    case  500: snprintf(width_str, sizeof(width_str), "06"); narrow = '0'; break;  /* narrow */
                    case 2400: snprintf(width_str, sizeof(width_str), "24"); narrow = '0'; break;  /* wide   */
                    default: return -RIG_EINVAL;
                }
                break;
            case RIG_MODE_LSB:
            case RIG_MODE_USB:
                switch (width) {
                    case 2400: snprintf(width_str, sizeof(width_str), "16"); narrow = '0'; break;  /* normal */
                    case 1800: snprintf(width_str, sizeof(width_str), "08"); narrow = '0'; break;  /* narrow */
                    case 3000: snprintf(width_str, sizeof(width_str), "25"); narrow = '0'; break;  /* wide   */
                    default: return -RIG_EINVAL;
                }
                break;
            case RIG_MODE_AM:
            case RIG_MODE_FM:
            case RIG_MODE_PKTFM:
                if (width < rig_passband_normal(rig, mode))
                    err = newcat_set_narrow(rig, vfo, TRUE);
                else
                    err = newcat_set_narrow(rig, vfo, FALSE);
                return err; 
            default:
                return -RIG_EINVAL;
        }   /* end switch(mode) */
    }   /* end else */

    rig_debug(RIG_DEBUG_TRACE, "sizeof(width_str) = %d\n", sizeof(width_str) );
    
    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "NA%c%c%cSH%c%s%c",
            main_sub_vfo, narrow, cat_term, main_sub_vfo, width_str, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

    /* Set RX Bandwidth */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));

    return err;
}


int newcat_get_rx_bandwidth(RIG *rig, vfo_t vfo, rmode_t mode, pbwidth_t *width)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    int ret_data_len;
    char *retlvl;
    int w;
    char cmd[] = "SH";
    char main_sub_vfo = '0';
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, cmd))
        return -RIG_ENAVAIL;

    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    if (newcat_is_rig(rig, RIG_MODEL_FT9000) || newcat_is_rig(rig, RIG_MODEL_FT2000))
        main_sub_vfo = (RIG_VFO_B == vfo) ? '1' : '0';

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c%c", cmd, main_sub_vfo, cat_term);

    /* Get RX BANDWIDTH */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s\n",
            __func__, err, priv->ret_data);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get RX_BANDWIDTH\n");
        return RIG_OK;
    }

    ret_data_len = strlen(priv->ret_data);

    /* skip command */
    retlvl = priv->ret_data + strlen(priv->cmd_str)-1;
    /* chop term */
    priv->ret_data[ret_data_len-1] = '\0';

    w = atoi(retlvl);   /*  width */

    if (newcat_is_rig(rig, RIG_MODEL_FT950)) {
        switch (mode) {
            case RIG_MODE_PKTUSB:
            case RIG_MODE_PKTLSB:
            case RIG_MODE_RTTY:
            case RIG_MODE_RTTYR:
            case RIG_MODE_CW:
            case RIG_MODE_CWR:
                switch (w) {
                    case 11: *width = 1700; break;  /* normal */
                    case  0:
                    case  7: *width =  500; break;  /* narrow */
                    case 13: *width = 2400; break;  /* wide   */
                    case 12: *width = 2000; break;
                    case 10: *width = 1400; break;
                    case  9: *width = 1200; break;
                    case  8: *width =  800; break;
                    case  6: *width =  400; break;
                    case  5: *width =  300; break;
                    case  3: *width =  100; break;
                    case  4: *width =  200; break;
                    default: return -RIG_EINVAL;
                }
                break;
            case RIG_MODE_LSB:
            case RIG_MODE_USB:
                switch (w) {
                    case  0:
                    case 13: *width = 2400; break;  /* normal */
                    case  9: *width = 1800; break;  /* narrow */
                    case 20: *width = 3000; break;  /* wide   */
                    case 19: *width = 2900; break;
                    case 18: *width = 2800; break;
                    case 17: *width = 2700; break;
                    case 16: *width = 2600; break;
                    case 15: *width = 2500; break;
                    case 14: *width = 2450; break;
                    case 12: *width = 2250; break;
                    case 11: *width = 2100; break;
                    case 10: *width = 1950; break;
                    case  8: *width = 1650; break;
                    case  7: *width = 1500; break;
                    case  6: *width = 1350; break;
                    case  5: *width = 1100; break;
                    case  4: *width =  850; break;
                    case  3: *width =  600; break;
                    case  2: *width =  400; break;
                    case  1: *width =  200; break;
                    default: return -RIG_EINVAL;
                } 
                break;
            case RIG_MODE_AM:
            case RIG_MODE_PKTFM:
            case RIG_MODE_FM:
                return RIG_OK; 
            default:
                return -RIG_EINVAL;
        }   /* end switch(mode) */

    }   /* end if FT950 */
    else {
        /* FT450, FT2000, FT9000 */
        switch (mode) {
            case RIG_MODE_PKTUSB:
            case RIG_MODE_PKTLSB:
            case RIG_MODE_RTTY:
            case RIG_MODE_RTTYR:
            case RIG_MODE_CW:
            case RIG_MODE_CWR:
            case RIG_MODE_LSB:
            case RIG_MODE_USB:
                if (w < 16)
                    *width = rig_passband_narrow(rig, mode);
                else if (w > 16)
                    *width = rig_passband_wide(rig, mode);
                else
                    *width = rig_passband_normal(rig, mode);
                break;
            case RIG_MODE_AM:
            case RIG_MODE_PKTFM:
            case RIG_MODE_FM:
                return RIG_OK;
            default:
                return -RIG_EINVAL;
        }   /* end switch (mode) */
    }   /* end else */

    return RIG_OK;
}


int newcat_set_faststep(RIG * rig, ncboolean fast_step)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char c;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "FS"))
        return -RIG_ENAVAIL;

    if (fast_step == TRUE) 
        c = '1';
    else 
        c = '0';

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "FS%c%c", c, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "cmd_str = %s\n", priv->cmd_str);

    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));

    return err;
}


int newcat_get_faststep(RIG * rig, ncboolean * fast_step)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err;
    char c;
    char command[] = "FS";
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, command))
        return -RIG_ENAVAIL;

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "%s%c", command, cat_term);
    /* Get Fast Step */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, priv->ret_data, sizeof(priv->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, priv->ret_data[strlen(priv->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, priv->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, FASTSTEP value = %c\n", __func__, err, priv->ret_data, priv->ret_data[2]);

    /* Check for I don't know this command? */
    if (strcmp(priv->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get FASTSTEP\n");
        return RIG_OK;
    }

    c = priv->ret_data[2];
    if (c == '1')
        *fast_step = TRUE;
    else
        *fast_step = FALSE;

    return RIG_OK;
}


int newcat_get_rigid(RIG * rig)
{
    struct newcat_priv_data *priv;
    const char * s;

    priv = (struct newcat_priv_data *)rig->state.priv;
    s = NULL;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    /* if first valid get */
    if (priv->rig_id == NC_RIGID_NONE) {
        s = newcat_get_info(rig);
        if (s != NULL) {
            s +=2;      /* ID0310, jump past ID */
            priv->rig_id = atoi(s);
        }
    }

    rig_debug(RIG_DEBUG_TRACE, "rig_id = %d, *s = %s\n", priv->rig_id, s);

    return priv->rig_id;
}


/*
 * input:   RIG *, vfo_t *
 * output:  VFO mode: RIG_VFO_VFO for VFO A and B
 *                    RIG_VFO_MEM for VFO MEM
 * return: RIG_OK or error 
 */
int newcat_get_vfo_mode(RIG * rig, vfo_t * vfo_mode)
{
    struct rig_state *state;
    char * retval;
    int err;
    newcat_cmd_data_t cmd;
    
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "IF")) 
        return -RIG_ENAVAIL;

    snprintf(cmd.cmd_str, sizeof(cmd.cmd_str), "IF%c", cat_term);

    /* Get VFO A Information ****************** */
    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, cmd.cmd_str);

    err = newcat_get_cmd(rig, &cmd);
    if (err != RIG_OK)
        return err;

    /* vfo, mem, P7 ************************** */
    retval = cmd.ret_data + 21;
    switch (*retval) {
        case '0': *vfo_mode = RIG_VFO_VFO; break;
        case '1':   /* Memory */
        case '2':   /* Memory Tune */
        case '3':   /* Quick Memory Bank */
        case '4':   /* Quick Memory Bank Tune */
        default:
                  *vfo_mode = RIG_VFO_MEM;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: vfo mode = %d\n", __func__, *vfo_mode);

    return RIG_OK;
}


/*
 * Writes cmd and does not wait for reply, no reply expected
 * input:  complete CAT command string including termination in cmd_str
 * return: RIG_OK or error 
 */
int newcat_set_cmd(RIG * rig, newcat_cmd_data_t * cmd)
{
    struct rig_state *state;
    int err;

    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, cmd->cmd_str);

    err = write_block(&state->rigport, cmd->cmd_str, strlen(cmd->cmd_str));

    return RIG_OK;
}


/*
 * Writed data and waits for responce
 * input:  complete CAT command string including termination in cmd_str
 * output: complete CAT command answer string in ret_data
 * return: RIG_OK or error 
 */
int newcat_get_cmd(RIG * rig, newcat_cmd_data_t * cmd)
{
    struct rig_state *state;
    int err;
    
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, cmd->cmd_str);

    err = write_block(&state->rigport, cmd->cmd_str, strlen(cmd->cmd_str));
    if (err != RIG_OK)
        return err;

    err = read_string(&state->rigport, cmd->ret_data, sizeof(cmd->ret_data), &cat_term, sizeof(cat_term));
    if (err < 0)
        return err;

    /* Check that command termination is correct */
    if (strchr(&cat_term, cmd->ret_data[strlen(cmd->ret_data) - 1]) == NULL) {
        rig_debug(RIG_DEBUG_ERR, "%s: Command is not correctly terminated '%s'\n", __func__, cmd->ret_data);

        return -RIG_EPROTO;
    }

    rig_debug(RIG_DEBUG_TRACE, "%s: read count = %d, ret_data = %s, ret_data length = %d\n", __func__, err, cmd->ret_data, strlen(cmd->ret_data));

    /* Check for I don't know this command? */
    if (strcmp(cmd->ret_data, cat_unknown_cmd) == 0) {
        rig_debug(RIG_DEBUG_TRACE, "Unrecognized command, get cmd = %s\n", cmd->cmd_str);
        return RIG_OK;
    }

    return RIG_OK;
}


int newcat_set_any_mem(RIG * rig, vfo_t vfo, int ch)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err, i;
    ncboolean restore_vfo;
    chan_t *chan_list;
    channel_t valid_chan;
    channel_cap_t *mem_caps = NULL;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "MC"))
        return -RIG_ENAVAIL;

    chan_list = rig->caps->chan_list;

    for (i=0; i<CHANLSTSIZ && !RIG_IS_CHAN_END(chan_list[i]); i++) {
        if (ch >= chan_list[i].start &&
                ch <= chan_list[i].end) {
            mem_caps = &chan_list[i].mem_caps;
            break;
        }
    }

    /* Test for valid usable channel, skip if empty */
    memset(&valid_chan, 0, sizeof(channel_t));
    valid_chan.channel_num = ch;
    err = newcat_get_channel(rig, &valid_chan);
    if (valid_chan.freq <= 1.0)
      mem_caps = NULL; 

    rig_debug(RIG_DEBUG_TRACE, "ValChan Freq = %d, pMemCaps = %d\n", valid_chan.freq, mem_caps);

    /* Out of Range, or empty */
    if (!mem_caps)
        return -RIG_ENAVAIL;

    /* set to usable vfo if needed */
    err = newcat_set_vfo_from_alias(rig, &vfo);
    if (err < 0)
        return err;

    /* Restore to VFO mode or leave in Memory Mode */
    switch (vfo) {
        case RIG_VFO_A:
            /* Jump back from memory channel */
            restore_vfo = TRUE;
            break;
        case RIG_VFO_MEM:
            /* Jump from channel to channel in memmory mode */
            restore_vfo = FALSE;
            break;
        case RIG_VFO_B:
        default:
            /* Only works with VFO A */
            return -RIG_ENTARGET;
    }


    /* Set Memory Channel Number ************** */
    rig_debug(RIG_DEBUG_TRACE, "channel_num = %d, vfo = %d\n", ch, vfo);

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "MC%03d%c", ch, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
      return err;

    /* Restore VFO even if setting to blank memory channel */
    if (restore_vfo) {
        err = newcat_vfomem_toggle(rig);
        if (err != RIG_OK)
            return err;
    }

    return RIG_OK;
}


int newcat_set_any_channel(RIG * rig, const channel_t * chan)
{
    struct newcat_priv_data *priv;
    struct rig_state *state;
    int err, i;
    int rxit;
    char c_rit, c_xit, c_mode, c_vfo, c_tone, c_rptr_shift;
    tone_t tone;
    ncboolean restore_vfo;
    chan_t *chan_list;
    channel_cap_t *mem_caps = NULL;
    
    priv = (struct newcat_priv_data *)rig->state.priv;
    state = &rig->state;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);


    if (!newcat_valid_command(rig, "MW"))
        return -RIG_ENAVAIL;

    chan_list = rig->caps->chan_list;

    for (i=0; i<CHANLSTSIZ && !RIG_IS_CHAN_END(chan_list[i]); i++) {
        if (chan->channel_num >= chan_list[i].start &&
                chan->channel_num <= chan_list[i].end &&
                // writable memory types... NOT 60M or READ only channels
                (chan_list[i].type == RIG_MTYPE_MEM ||
                 chan_list[i].type == RIG_MTYPE_EDGE) ) {
            mem_caps = &chan_list[i].mem_caps;
            break;
        }
    }

    /* Out of Range */
    if (!mem_caps)
        return -RIG_ENAVAIL;

    /* Set Restore to VFO or leave in memory mode */
    switch (priv->current_vfo) {
        case RIG_VFO_A:
            /* Jump back from memory channel */
            restore_vfo = TRUE;
            break;
        case RIG_VFO_MEM:
            /* Jump from channel to channel in memmory mode */
            restore_vfo = FALSE; 
            break;
        case RIG_VFO_B:
        default:
            /* Only works with VFO A */
            return -RIG_ENTARGET;
    }

    /* Write Memory Channel ************************* */
    /*  Clarifier TX, RX */
    if (chan->rit) {
        rxit = chan->rit;
        c_rit = '1';
        c_xit = '0';
    } else if (chan->xit) {
        rxit= chan->xit;
        c_rit = '0';
        c_xit = '1';
    } else { 
        rxit  =  0;
        c_rit = '0';
        c_xit = '0';
    }

    /* MODE */
    switch(chan->mode) {
        case RIG_MODE_LSB:    c_mode = '1'; break;
        case RIG_MODE_USB:    c_mode = '2'; break;
        case RIG_MODE_CW:     c_mode = '3'; break;
        case RIG_MODE_FM:     c_mode = '4'; break;
        case RIG_MODE_AM:     c_mode = '5'; break;
        case RIG_MODE_RTTY:   c_mode = '6'; break;
        case RIG_MODE_CWR:    c_mode = '7'; break;
        case RIG_MODE_PKTLSB: c_mode = '8'; break;
        case RIG_MODE_RTTYR:  c_mode = '9'; break;
        case RIG_MODE_PKTFM:  c_mode = 'A'; break;
        case RIG_MODE_PKTUSB: c_mode = 'C'; break;
        default: c_mode = '1'; break;
    }

    /* VFO Fixed */
    c_vfo = '0';

    /* CTCSS Tone / Sql */
    if (chan->ctcss_tone) {
        c_tone = '2';
        tone = chan->ctcss_tone;
    } else if (chan->ctcss_sql) {
        c_tone = '1';
        tone = chan->ctcss_sql;
    } else {
        c_tone = '0'; 
        tone = 0;
    }
    for (i = 0; rig->caps->ctcss_list[i] != 0; i++)
        if (tone == rig->caps->ctcss_list[i]) {
            tone = i;
            if (tone > 49)
                tone = 0;
            break;
        }

    /* Repeater Shift */
    switch (chan->rptr_shift) {
        case RIG_RPT_SHIFT_NONE:  c_rptr_shift = '0'; break;
        case RIG_RPT_SHIFT_PLUS:  c_rptr_shift = '1'; break;
        case RIG_RPT_SHIFT_MINUS: c_rptr_shift = '2'; break;
        default: c_rptr_shift = '0';
    }

    snprintf(priv->cmd_str, sizeof(priv->cmd_str), "MW%03d%08d%+.4d%c%c%c%c%c%02d%c%c",
            chan->channel_num, (int)chan->freq, rxit, c_rit, c_xit, c_mode, c_vfo,
            c_tone, tone, c_rptr_shift, cat_term);

    rig_debug(RIG_DEBUG_TRACE, "%s: cmd_str = %s\n", __func__, priv->cmd_str);

    /* Set Memory Channel */
    err = write_block(&state->rigport, priv->cmd_str, strlen(priv->cmd_str));
    if (err != RIG_OK)
        return err;

    /* Restore VFO ********************************** */
    if (restore_vfo) {
        err = newcat_vfomem_toggle(rig);
        return err;
    }

    return RIG_OK;
}


int newcat_vfomem_toggle(RIG * rig)
{
    int err;
    newcat_cmd_data_t cmd;

    rig_debug(RIG_DEBUG_VERBOSE, "%s called\n", __func__);

    if (!newcat_valid_command(rig, "VM"))
        return -RIG_ENAVAIL;

    /* copy set command */
    snprintf(cmd.cmd_str, sizeof(cmd.cmd_str), "%s", "VM;");

    err = newcat_set_cmd(rig, &cmd);

    return err;
}
