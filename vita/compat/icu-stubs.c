/*
 * VitaSDK's ICU is built without break iteration (UCONFIG_NO_BREAK_ITERATION), but its Boost.Locale
 * was built against a full ICU, so Boost.Locale's ICU backend refers to these. They back word and
 * line boundary analysis and title-casing, none of which Wesnoth uses, so here they report
 * "unsupported" the way ICU itself would.
 *
 * Plain C with the C++ symbol names: ICU's headers do not declare these when break iteration is
 * configured out, so they cannot be defined as the C++ members they are.
 */
#include <stddef.h>
#include <stdint.h>

/* UErrorCode: an int, failures are > 0. */
#define U_UNSUPPORTED_ERROR 16

static void *unsupported(int *status)
{
	if (status && *status <= 0) {
		*status = U_UNSUPPORTED_ERROR;
	}
	return NULL;
}

/* icu::BreakIterator::create*Instance(const icu::Locale&, UErrorCode&) */
void *vita_icu_line(const void *locale, int *status) __asm__("_ZN6icu_7313BreakIterator18createLineInstanceERKNS_6LocaleER10UErrorCode");
void *vita_icu_word(const void *locale, int *status) __asm__("_ZN6icu_7313BreakIterator18createWordInstanceERKNS_6LocaleER10UErrorCode");
void *vita_icu_sentence(const void *locale, int *status) __asm__("_ZN6icu_7313BreakIterator22createSentenceInstanceERKNS_6LocaleER10UErrorCode");
void *vita_icu_character(const void *locale, int *status) __asm__("_ZN6icu_7313BreakIterator23createCharacterInstanceERKNS_6LocaleER10UErrorCode");

void *vita_icu_line(const void *locale, int *status) { (void)locale; return unsupported(status); }
void *vita_icu_word(const void *locale, int *status) { (void)locale; return unsupported(status); }
void *vita_icu_sentence(const void *locale, int *status) { (void)locale; return unsupported(status); }
void *vita_icu_character(const void *locale, int *status) { (void)locale; return unsupported(status); }

/* icu::UnicodeString& icu::UnicodeString::toTitle(icu::BreakIterator*, const icu::Locale&): the
 * string is left as it is. */
void *vita_icu_to_title(void *self, void *iter, const void *locale) __asm__("_ZN6icu_7313UnicodeString7toTitleEPNS_13BreakIteratorERKNS_6LocaleE");
void *vita_icu_to_title(void *self, void *iter, const void *locale) { (void)iter; (void)locale; return self; }

/* int32_t ucasemap_utf8ToTitle(UCaseMap*, char*, int32_t, const char*, int32_t, UErrorCode*) */
int32_t ucasemap_utf8ToTitle_73(void *map, char *dest, int32_t capacity, const char *src, int32_t length, int *status);
int32_t ucasemap_utf8ToTitle_73(void *map, char *dest, int32_t capacity, const char *src, int32_t length, int *status)
{
	(void)map; (void)dest; (void)capacity; (void)src; (void)length;
	unsupported(status);
	return 0;
}
