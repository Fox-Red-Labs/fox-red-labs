/* jsmn parsing a 300 byte JSON document 100 times. Split out of the original text image (L1306 port). */
#include <string.h>
#include "tb.h"
#define JSMN_STATIC
#include "jsmn.h"
#include "json_doc.h"

int main(void)
{
	uint32_t c;

	tb_start("text_jsmn");

	/* ---- jsmn ---- */
	{
		static jsmntok_t tok[64];
		jsmn_parser p;
		int n = 0;
		TB_BEST(c, 3, { for (int i = 0; i < 100; i++) { jsmn_init(&p); n = jsmn_parse(&p, JSON_DOC, JSON_DOC_LEN, tok, 64); } });
		TB_NOTE(n3);
		tb_ns(&n3, "per_parse_of_300B;tokens="); tb_nu(&n3, (uint32_t)n);
		tb_ns(&n3, ";total_for_100=");
		tb_nu(&n3, c);
		tb_result("jsmn", "parse_300B_x100", c / 100U, n == JSON_TOKENS && tok[0].type == JSMN_OBJECT, n3.b);
	}
	tb_done();
}
