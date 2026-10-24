#pragma once
#include "Support.Tests.MarkdownHtml.h"

namespace MarkdownInlineTests
{
using namespace DxUi::RichText;
inline constexpr std::wstring_view paddedTable = L"| a | b | c |\n| - | - | - |\n| x |\n\ntail\n";
inline std::vector<std::uint64_t> TableCellIdentities(const MarkdownSyntax& syntax)
{
    std::vector<std::uint64_t> identities;
    for (const auto& node : syntax.Nodes())
        if (node.block.kind == MarkdownBlockKind::TableCell)
            identities.push_back(node.identity);
    return identities;
}
inline bool UniqueNodeIdentities(const MarkdownSyntax& syntax)
{
    std::vector<std::uint64_t> identities;
    for (const auto& node : syntax.Nodes())
        identities.push_back(node.identity);
    std::sort(identities.begin(), identities.end());
    return ! identities.empty() && identities.front() != 0u && std::adjacent_find(identities.begin(), identities.end()) == identities.end();
}
struct Scenario
{
    std::wstring_view name, source, common, gfm, rich;
};
inline constexpr Scenario scenarios[] = {
    {L"reference-only paragraph cannot become a table header", L"[r]: /url\n| - |\n", L"<p>| - |</p>\n", {}, {}},
    {L"reference extraction before a real table header",
     L"[r]: /url\nheader\n| --- |\n[r]\n",
     L"<p>header\n| --- |\n<a href=\"/url\">r</a></p>\n",
     L"<table>\n<thead>\n<tr>\n<th>header</th>\n</tr>\n</thead>\n<tbody>\n<tr>\n<td><a href=\"/url\">r</a></td>\n</tr>\n</tbody>\n</table>\n",
     {}},
    {L"retired table candidate inside a quote",
     L"> [r]: /url\n> | - |\n> [r]\n",
     L"<blockquote>\n<p>| - |\n<a href=\"/url\">r</a></p>\n</blockquote>\n",
     {},
     {}},
    {L"GFM tagfilter VT whitespace and slash boundaries",
     L"<div>\n<script/unknown> <script/> <SCRIPT\v>\n</div>\n",
     L"<div>\n<script/unknown> <script/> <SCRIPT\v>\n</div>\n",
     L"<div>\n<script/unknown> &lt;script/> &lt;SCRIPT\v>\n</div>\n",
     {}},
    {L"strict GFM domain differs from reference runtime",
     L"http://localhost/path https://a..b/path\n",
     L"<p>http://localhost/path https://a..b/path</p>\n",
     {},
     {}},
    {L"GFM task marker whitespace follows specified grammar",
     L"- [\t] text\n",
     L"<ul>\n<li>[\t] text</li>\n</ul>\n",
     L"<ul>\n<li><input disabled=\"\" type=\"checkbox\"> text</li>\n</ul>\n",
     {}},
    {L"extended URL owns interior emphasis markers",
     L"www.example.com*em*\n",
     L"<p>www.example.com<em>em</em></p>\n",
     L"<p><a href=\"http://www.example.com*em\">www.example.com*em</a>*</p>\n",
     {}},
    {L"extended URLs retain literal references and escapes",
     L"http://a.com/?x=&amp;y=2 http://a.com/a\\*b\n",
     L"<p>http://a.com/?x=&amp;y=2 http://a.com/a*b</p>\n",
     L"<p><a href=\"http://a.com/?x=&amp;amp;y=2\">http://a.com/?x=&amp;amp;y=2</a> <a href=\"http://a.com/a%5C*b\">http://a.com/a\\*b</a></p>\n",
     {}},
    {L"entity whitespace can precede an extended URL",
     L"&#32;http://a.com\n",
     L"<p> http://a.com</p>\n",
     L"<p> <a href=\"http://a.com\">http://a.com</a></p>\n",
     {}},
    {L"explicit links own extended URL text", L"[https://example.com](destination)\n", L"<p><a href=\"destination\">https://example.com</a></p>\n", {}, {}},
    {L"extended email after emphasis resolution",
     L"*a@example.com* _a@example.com_\n",
     L"<p><em>a@example.com</em> <em>a@example.com</em></p>\n",
     L"<p><em><a href=\"mailto:a@example.com\">a@example.com</a></em> <em><a href=\"mailto:a@example.com\">a@example.com</a></em></p>\n",
     {}},
    {L"Unicode domain and uppercase scheme",
     L"HTTPS://例子.测试/path\n",
     L"<p>HTTPS://例子.测试/path</p>\n",
     L"<p><a href=\"HTTPS://%E4%BE%8B%E5%AD%90.%E6%B5%8B%E8%AF%95/path\">HTTPS://例子.测试/path</a></p>\n",
     {}},
    {L"GFM label whitespace dialect", L"[a\fb]\n\n[a b]: /one\n", L"<p>[a\fb]</p>\n", L"<p><a href=\"/one\">a\fb</a></p>\n", L"<p>[a\fb]</p>\n"},
    {L"GFM link whitespace dialect", L"[a](\f/url)\n", L"<p>[a](\f/url)</p>\n", L"<p><a href=\"/url\">a</a></p>\n", L"<p>[a](\f/url)</p>\n"},
    {L"non-ASCII destination space", L"[r]\n\n[r]: /a\u00a0b\n", L"<p><a href=\"/a%C2%A0b\">r</a></p>\n", {}, {}},
    {L"case folding does not normalize combining sequences", L"[é] [e\u0301]\n\n[É]: /one\n", L"<p><a href=\"/one\">é</a> [e\u0301]</p>\n", {}, {}},
    {L"empty task requires content whitespace",
     L"- [ ]\n- [ ] \n",
     L"<ul>\n<li>[ ]</li>\n<li>[ ]</li>\n</ul>\n",
     L"<ul>\n<li>[ ]</li>\n<li><input disabled=\"\" type=\"checkbox\"> </li>\n</ul>\n",
     {}},
    {L"context precedence", L"\\*a\\* &amp; `&amp; *b*`\n", L"<p>*a* &amp; <code>&amp;amp; *b*</code></p>\n", {}, {}},
    {L"code delimiter and whitespace normalization", L"`` a ` b\nc ``\n", L"<p><code>a ` b c</code></p>\n", {}, {}},
    {L"Unicode punctuation and intraword underscore", L"foo_bar_baz «*café*»\n", L"<p>foo_bar_baz «<em>café</em>»</p>\n", {}, {}},
    {L"supplementary symbol flanking dialect", L"a*\U0001f600*b\n", L"<p>a*\U0001f600*b</p>\n", L"<p>a<em>\U0001f600</em>b</p>\n", L"<p>a*\U0001f600*b</p>\n"},
    {L"nested delimiters", L"***both*** and **bold *em***\n", L"<p><em><strong>both</strong></em> and <strong>bold <em>em</em></strong></p>\n", {}, {}},
    {L"GFM printed redundant strong convention",
     L"****bold****\n",
     L"<p><strong><strong>bold</strong></strong></p>\n",
     L"<p><strong>bold</strong></p>\n",
     L"<p><strong><strong>bold</strong></strong></p>\n"},
    {L"link precedence over emphasis", L"*[label*](url \"title\")\n", L"<p>*<a href=\"url\" title=\"title\">label*</a></p>\n", {}, {}},
    {L"nested link exclusion", L"[outer [inner](b)](a)\n", L"<p>[outer <a href=\"b\">inner</a>](a)</p>\n", {}, {}},
    {L"image alternative text",
     L"[![*café*](image \"alt title\")](link)\n",
     L"<p><a href=\"link\"><img src=\"image\" alt=\"café\" title=\"alt title\" /></a></p>\n",
     {},
     {}},
    {L"balanced destination and escaped title", L"[a](a(b(c)) \"a &amp; b\")\n", L"<p><a href=\"a(b(c))\" title=\"a &amp; b\">a</a></p>\n", {}, {}},
    {L"global forward reference variants",
     L"[r][ref] [ref][] [ref]\n\n[REF]: /one 'first'\n[ref]: /two\n",
     L"<p><a href=\"/one\" title=\"first\">r</a> <a href=\"/one\" title=\"first\">ref</a> <a href=\"/one\" title=\"first\">ref</a></p>\n",
     {},
     {}},
    {L"full Unicode default folding",
     L"[STRASSE] [\U00010428]\n\n[Straße]: /one\n[\U00010400]: /two\n",
     L"<p><a href=\"/one\">STRASSE</a> <a href=\"/two\">\U00010428</a></p>\n",
     {},
     {}},
    {L"label whitespace is ASCII source whitespace", L"[a\u00a0b] [a b]\n\n[a\u00a0b]: /one\n", L"<p><a href=\"/one\">a\u00a0b</a> [a b]</p>\n", {}, {}},
    {L"angle URI and email",
     L"<https://example.invalid/a?x=1&y=2> <a@example.invalid>\n",
     L"<p><a href=\"https://example.invalid/a?x=1&amp;y=2\">https://example.invalid/a?x=1&amp;y=2</a> <a "
     L"href=\"mailto:a@example.invalid\">a@example.invalid</a></p>\n",
     {},
     {}},
    {L"soft and hard breaks", L"a\nb  \nc\\\nd\n", L"<p>a\nb<br />\nc<br />\nd</p>\n", {}, {}},
    {L"raw HTML opacity", L"x <span title=\"&amp; *literal*\">&amp;</span>\n", L"<p>x <span title=\"&amp; *literal*\">&amp;</span></p>\n", {}, {}},
    {L"short HTML comments", L"x <!--> y -->\n", L"<p>x <!--> y --&gt;</p>\n", {}, {}},
    {L"inline declaration dialect", L"x <!foo>\n", L"<p>x <!foo></p>\n", L"<p>x &lt;!foo&gt;</p>\n", L"<p>x <!foo></p>\n"},
    {L"GFM two-tilde strike",
     L"~~a **b**~~ ~c~ ~~~d~~~\n",
     L"<p>~~a <strong>b</strong>~~ ~c~ ~~~d~~~</p>\n",
     L"<p><del>a <strong>b</strong></del> ~c~ ~~~d~~~</p>\n",
     {}},
    {L"GFM task and ordinary list items",
     L"- [x] yes\n- [ ] no\n- [x]word\n",
     L"<ul>\n<li>[x] yes</li>\n<li>[ ] no</li>\n<li>[x]word</li>\n</ul>\n",
     L"<ul>\n<li><input checked=\"\" disabled=\"\" type=\"checkbox\"> yes</li>\n<li><input disabled=\"\" type=\"checkbox\"> no</li>\n<li>[x]word</li>\n</ul>\n",
     {}},
    {L"GFM table alignment and escaped code pipe",
     L"| left | right |\n| :--- | ---: |\n| a | `b\\|c` |\n| d |\n",
     L"<p>| left | right |\n| :--- | ---: |\n| a | <code>b\\|c</code> |\n| d |</p>\n",
     L"<table>\n<thead>\n<tr>\n<th align=\"left\">left</th>\n<th align=\"right\">right</th>\n</tr>\n</thead>\n<tbody>\n<tr>\n<td align=\"left\">a</td>\n<td "
     L"align=\"right\"><code>b|c</code></td>\n</tr>\n<tr>\n<td align=\"left\">d</td>\n<td align=\"right\"></td>\n</tr>\n</tbody>\n</table>\n",
     {}},
    {L"GFM extended URL punctuation",
     L"(https://example.invalid/a_(b)).\n",
     L"<p>(https://example.invalid/a_(b)).</p>\n",
     L"<p>(<a href=\"https://example.invalid/a_(b)\">https://example.invalid/a_(b)</a>).</p>\n",
     {}},
    {L"GFM extended mail and www",
     L"www.example.invalid a@example.invalid\n",
     L"<p>www.example.invalid a@example.invalid</p>\n",
     L"<p><a href=\"http://www.example.invalid\">www.example.invalid</a> <a href=\"mailto:a@example.invalid\">a@example.invalid</a></p>\n",
     {}},
    {L"GFM raw block tagfilter",
     L"<script>\n**literal** &amp;\n</script>\n",
     L"<script>\n**literal** &amp;\n</script>\n",
     L"&lt;script>\n**literal** &amp;\n&lt;/script>\n",
     {}},
    {L"GFM inline tagfilter and boundaries",
     L"x <TITLE> a </TITLE> <title-widget> b\n",
     L"<p>x <TITLE> a </TITLE> <title-widget> b</p>\n",
     L"<p>x &lt;TITLE> a &lt;/TITLE> <title-widget> b</p>\n",
     {}},
};
inline std::wstring_view Expected(const Scenario& scenario, MarkdownProfile profile)
{
    if (profile == MarkdownProfile::CommonMark0312)
        return scenario.common;
    const auto gfm = scenario.gfm.empty() ? scenario.common : scenario.gfm;
    return profile == MarkdownProfile::RichMarkdown && ! scenario.rich.empty() ? scenario.rich : gfm;
}
} // namespace MarkdownInlineTests
