#include "EXTERN.h"
#include "perl.h"
#include "XSUB.h"

#include "RPM.h"

static char * const rcsid = "$Id: RPM.xs,v 1.2 2000/05/27 05:22:51 rjray Exp $";

extern XS(boot_RPM__Constants);
extern XS(boot_RPM__Header);
extern XS(boot_RPM__Database);

static HV* tag2num_priv;
static HV* num2tag_priv;
static SV* errSV;
static CV* err_callback;

static void setup_tag_mappings(void)
{
    const char* tag;
    int num;
    int idx;
    char str_num[8];

    tag2num_priv = perl_get_hv("RPM::tag2num", TRUE);
    num2tag_priv = perl_get_hv("RPM::num2tag", TRUE);
    for (idx = 0; idx < rpmTagTableSize; idx++)
    {
        //
        // For future reference: The offset of 7 used in referring to the
        // (const char *) tag and its length is to discard the "RPMTAG_"
        // prefix inherent in the tag names.
        //
        tag = rpmTagTable[idx].name;
        num = rpmTagTable[idx].val;
        hv_store(tag2num_priv, (char *)tag + 7, strlen(tag) - 7,
                 newSViv(num), FALSE);
        Zero(str_num, 1, 8);
        snprintf(str_num, 8, "%d", num);
        hv_store(num2tag_priv, str_num, strlen(str_num),
                 newSVpv((char *)tag + 7, strlen(tag) - 7), FALSE);
    }
}

int tag2num(const char* tag)
{
    SV** svp;

    // Get the #define value for the tag from the hash made at boot-up
    svp = hv_fetch(tag2num_priv, (char *)tag, strlen(tag), FALSE);
    if (! (svp && SvOK(*svp) && SvIOK(*svp)))
        // Later we may need to set some sort of error message
        return 0;

    return (SvIV(*svp));
}

const char* num2tag(int num)
{
    SV** svp;
    STRLEN na;
    char str_num[8];
    SV* tmp;

    Zero(str_num, 1, 8);
    snprintf(str_num, 8, "%d", num);
    svp = hv_fetch(num2tag_priv, str_num, strlen(str_num), FALSE);
    if (! (svp && SvPOK(*svp)))
        return Nullch;

    return (SvPV(*svp, na));
}

char* rpm_GetOsName(void)
{
    char* os_name;
    int os_val;

    rpmGetOsInfo((const char **)&os_name, &os_val);
    return os_name;
}

char* rpm_GetArchName(void)
{
    char* arch_name;
    int arch_val;

    rpmGetArchInfo((const char **)&arch_name, &arch_val);
    return arch_name;
}

// This is a callback routine that the bootstrapper will register with the RPM
// lib so as to catch any errors. (I hope)
static void rpm_catch_errors(void)
{
    int error_code;
    char* error_string;

    error_code = rpmErrorCode();
    error_string = rpmErrorString();

    // Set the string part, first
    sv_setsv(errSV, newSVpv(error_string, strlen(error_string)));
    // Set the IV part
    sv_setiv(errSV, error_code);
    // Doing that didn't erase the PV part, but it cleared the flag:
    SvPOK_on(errSV);

    // If there is a current callback, invoke it:
    if (err_callback != NULL)
    {
        // This is just standard boilerplate for calling perl from C
        dSP;
        ENTER;
        SAVETMPS;
        PUSHMARK(sp);
        XPUSHs(sv_2mortal(newSViv(error_code)));
        XPUSHs(sv_2mortal(newSVpv(error_string, strlen(error_string))));
        PUTBACK;

        // The actual call
        perl_call_sv((SV *)err_callback, G_DISCARD);

        // More boilerplate
        SPAGAIN;
        PUTBACK;
        FREETMPS;
        LEAVE;
    }

    return;
}

// This is just to make available an easy way to clear both sides of $RPM::err
void clear_errors(void)
{
    sv_setsv(errSV, newSVpv("", 0));
    sv_setiv(errSV, 0);
    SvPOK_on(errSV);

    return;
}

SV* set_error_callback(SV* newcb)
{
    CV* oldcb;

    oldcb = err_callback;

    if (SvROK(newcb)) newcb = SvRV(newcb);
    if (SvTYPE(newcb) == SVt_PVCV)
        err_callback = (CV *)newcb;
    else if (SvPOK(newcb))
    {
        char* fn_name;
        char* sv_name;
        STRLEN len;

        sv_name = SvPV(newcb, len);
        if (! strstr(sv_name, "::"))
        {
            Newz(TRUE, fn_name, strlen(sv_name) + 7, char);
            strncat(fn_name, "main::", 6);
            strcat(fn_name + 6, sv_name);
        }
        else
            fn_name = sv_name;

        err_callback = perl_get_cv(fn_name, FALSE);
    }
    else
    {
        err_callback = Null(CV *);
    }

    return (SV *)oldcb;
}

void rpm_error(int code, const char* message)
{
    rpmError(code, (char *)message);
}


MODULE = RPM            PACKAGE = RPM::Error           


SV*
set_error_callback(newcb)
    SV* newcb;
    PROTOTYPE: $

void
clear_errors()
    PROTOTYPE:

void
rpm_error(code, message)
    int code;
    char* message;
    PROTOTYPE: $$


MODULE = RPM            PACKAGE = RPM           PREFIX = rpm_


char*
rpm_GetOsName()
    PROTOTYPE:

char*
rpm_GetArchName()
    PROTOTYPE:


BOOT:
{
    SV * config_loaded;

    errSV = perl_get_sv("RPM::err", GV_ADD|GV_ADDMULTI);
    config_loaded = perl_get_sv("RPM::__config_loaded", GV_ADD|GV_ADDMULTI);
    if (! (SvOK(config_loaded) && SvTRUE(config_loaded)))
    {
        rpmReadConfigFiles(NULL, NULL);
        sv_setiv(config_loaded, TRUE);
    }

    setup_tag_mappings();
    rpmErrorSetCallback(rpm_catch_errors);
    err_callback = Null(CV *);

    newXS("RPM::bootstrap_Constants", boot_RPM__Constants, file);
    newXS("RPM::bootstrap_Header", boot_RPM__Header, file);
    newXS("RPM::bootstrap_Database", boot_RPM__Database, file);
}
