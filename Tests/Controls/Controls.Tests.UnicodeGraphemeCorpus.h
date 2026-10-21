#pragma once
#include <cstdint>
#include <span>
#include <string_view>

// Unicode 17.0.0 GraphemeBreakTest.txt, all 766 original vectors, without exclusions.
// See ThirdParty/Unicode/17.0.0/LICENSE.txt. Generated; never hand edit vectors.
namespace UnicodeGraphemeCorpus
{
struct Case
{
    std::wstring_view text;
    std::span<const std::uint16_t> boundaries;
};
// clang-format off
inline constexpr std::uint16_t kEnds0[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds1[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds2[] = {0u, 2u};
inline constexpr std::uint16_t kEnds3[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds4[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds5[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds6[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds7[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds8[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds9[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds10[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds11[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds12[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds13[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds14[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds15[] = {0u, 1u, 2u, 4u};
inline constexpr std::uint16_t kEnds16[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds17[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds18[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds19[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds20[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds21[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds22[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds23[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds24[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds25[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds26[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds27[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds28[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds29[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds30[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds31[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds32[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds33[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds34[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds35[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds36[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds37[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds38[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds39[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds40[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds41[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds42[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds43[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds44[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds45[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds46[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds47[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds48[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds49[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds50[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds51[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds52[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds53[] = {0u, 1u, 2u, 4u};
inline constexpr std::uint16_t kEnds54[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds55[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds56[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds57[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds58[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds59[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds60[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds61[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds62[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds63[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds64[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds65[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds66[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds67[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds68[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds69[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds70[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds71[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds72[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds73[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds74[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds75[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds76[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds77[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds78[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds79[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds80[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds81[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds82[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds83[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds84[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds85[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds86[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds87[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds88[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds89[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds90[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds91[] = {0u, 1u, 2u, 4u};
inline constexpr std::uint16_t kEnds92[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds93[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds94[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds95[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds96[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds97[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds98[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds99[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds100[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds101[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds102[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds103[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds104[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds105[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds106[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds107[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds108[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds109[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds110[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds111[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds112[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds113[] = {0u, 1u, 2u, 3u};
inline constexpr std::uint16_t kEnds114[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds115[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds116[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds117[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds118[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds119[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds120[] = {0u, 2u};
inline constexpr std::uint16_t kEnds121[] = {0u, 3u};
inline constexpr std::uint16_t kEnds122[] = {0u, 2u};
inline constexpr std::uint16_t kEnds123[] = {0u, 3u};
inline constexpr std::uint16_t kEnds124[] = {0u, 2u};
inline constexpr std::uint16_t kEnds125[] = {0u, 3u};
inline constexpr std::uint16_t kEnds126[] = {0u, 2u};
inline constexpr std::uint16_t kEnds127[] = {0u, 3u};
inline constexpr std::uint16_t kEnds128[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds129[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds130[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds131[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds132[] = {0u, 2u};
inline constexpr std::uint16_t kEnds133[] = {0u, 3u};
inline constexpr std::uint16_t kEnds134[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds135[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds136[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds137[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds138[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds139[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds140[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds141[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds142[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds143[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds144[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds145[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds146[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds147[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds148[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds149[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds150[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds151[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds152[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds153[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds154[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds155[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds156[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds157[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds158[] = {0u, 2u};
inline constexpr std::uint16_t kEnds159[] = {0u, 3u};
inline constexpr std::uint16_t kEnds160[] = {0u, 2u};
inline constexpr std::uint16_t kEnds161[] = {0u, 3u};
inline constexpr std::uint16_t kEnds162[] = {0u, 2u};
inline constexpr std::uint16_t kEnds163[] = {0u, 3u};
inline constexpr std::uint16_t kEnds164[] = {0u, 2u};
inline constexpr std::uint16_t kEnds165[] = {0u, 3u};
inline constexpr std::uint16_t kEnds166[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds167[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds168[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds169[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds170[] = {0u, 2u};
inline constexpr std::uint16_t kEnds171[] = {0u, 3u};
inline constexpr std::uint16_t kEnds172[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds173[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds174[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds175[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds176[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds177[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds178[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds179[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds180[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds181[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds182[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds183[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds184[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds185[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds186[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds187[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds188[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds189[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds190[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds191[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds192[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds193[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds194[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds195[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds196[] = {0u, 2u};
inline constexpr std::uint16_t kEnds197[] = {0u, 3u};
inline constexpr std::uint16_t kEnds198[] = {0u, 2u};
inline constexpr std::uint16_t kEnds199[] = {0u, 3u};
inline constexpr std::uint16_t kEnds200[] = {0u, 2u};
inline constexpr std::uint16_t kEnds201[] = {0u, 3u};
inline constexpr std::uint16_t kEnds202[] = {0u, 2u};
inline constexpr std::uint16_t kEnds203[] = {0u, 3u};
inline constexpr std::uint16_t kEnds204[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds205[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds206[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds207[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds208[] = {0u, 2u};
inline constexpr std::uint16_t kEnds209[] = {0u, 3u};
inline constexpr std::uint16_t kEnds210[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds211[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds212[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds213[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds214[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds215[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds216[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds217[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds218[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds219[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds220[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds221[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds222[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds223[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds224[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds225[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds226[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds227[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds228[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds229[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds230[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds231[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds232[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds233[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds234[] = {0u, 2u};
inline constexpr std::uint16_t kEnds235[] = {0u, 3u};
inline constexpr std::uint16_t kEnds236[] = {0u, 2u};
inline constexpr std::uint16_t kEnds237[] = {0u, 3u};
inline constexpr std::uint16_t kEnds238[] = {0u, 2u};
inline constexpr std::uint16_t kEnds239[] = {0u, 3u};
inline constexpr std::uint16_t kEnds240[] = {0u, 2u};
inline constexpr std::uint16_t kEnds241[] = {0u, 3u};
inline constexpr std::uint16_t kEnds242[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds243[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds244[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds245[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds246[] = {0u, 2u};
inline constexpr std::uint16_t kEnds247[] = {0u, 3u};
inline constexpr std::uint16_t kEnds248[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds249[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds250[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds251[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds252[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds253[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds254[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds255[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds256[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds257[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds258[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds259[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds260[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds261[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds262[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds263[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds264[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds265[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds266[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds267[] = {0u, 3u, 4u};
inline constexpr std::uint16_t kEnds268[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds269[] = {0u, 3u, 4u};
inline constexpr std::uint16_t kEnds270[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds271[] = {0u, 3u, 4u};
inline constexpr std::uint16_t kEnds272[] = {0u, 3u};
inline constexpr std::uint16_t kEnds273[] = {0u, 4u};
inline constexpr std::uint16_t kEnds274[] = {0u, 3u};
inline constexpr std::uint16_t kEnds275[] = {0u, 4u};
inline constexpr std::uint16_t kEnds276[] = {0u, 3u};
inline constexpr std::uint16_t kEnds277[] = {0u, 4u};
inline constexpr std::uint16_t kEnds278[] = {0u, 3u};
inline constexpr std::uint16_t kEnds279[] = {0u, 4u};
inline constexpr std::uint16_t kEnds280[] = {0u, 4u};
inline constexpr std::uint16_t kEnds281[] = {0u, 3u, 5u};
inline constexpr std::uint16_t kEnds282[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds283[] = {0u, 3u, 4u};
inline constexpr std::uint16_t kEnds284[] = {0u, 3u};
inline constexpr std::uint16_t kEnds285[] = {0u, 4u};
inline constexpr std::uint16_t kEnds286[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds287[] = {0u, 3u, 4u};
inline constexpr std::uint16_t kEnds288[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds289[] = {0u, 3u, 4u};
inline constexpr std::uint16_t kEnds290[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds291[] = {0u, 3u, 4u};
inline constexpr std::uint16_t kEnds292[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds293[] = {0u, 3u, 4u};
inline constexpr std::uint16_t kEnds294[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds295[] = {0u, 3u, 4u};
inline constexpr std::uint16_t kEnds296[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds297[] = {0u, 3u, 4u};
inline constexpr std::uint16_t kEnds298[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds299[] = {0u, 3u, 4u};
inline constexpr std::uint16_t kEnds300[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds301[] = {0u, 3u, 4u};
inline constexpr std::uint16_t kEnds302[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds303[] = {0u, 3u, 4u};
inline constexpr std::uint16_t kEnds304[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds305[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds306[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds307[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds308[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds309[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds310[] = {0u, 2u};
inline constexpr std::uint16_t kEnds311[] = {0u, 3u};
inline constexpr std::uint16_t kEnds312[] = {0u, 2u};
inline constexpr std::uint16_t kEnds313[] = {0u, 3u};
inline constexpr std::uint16_t kEnds314[] = {0u, 2u};
inline constexpr std::uint16_t kEnds315[] = {0u, 3u};
inline constexpr std::uint16_t kEnds316[] = {0u, 2u};
inline constexpr std::uint16_t kEnds317[] = {0u, 3u};
inline constexpr std::uint16_t kEnds318[] = {0u, 3u};
inline constexpr std::uint16_t kEnds319[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds320[] = {0u, 2u};
inline constexpr std::uint16_t kEnds321[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds322[] = {0u, 2u};
inline constexpr std::uint16_t kEnds323[] = {0u, 3u};
inline constexpr std::uint16_t kEnds324[] = {0u, 2u};
inline constexpr std::uint16_t kEnds325[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds326[] = {0u, 2u};
inline constexpr std::uint16_t kEnds327[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds328[] = {0u, 2u};
inline constexpr std::uint16_t kEnds329[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds330[] = {0u, 2u};
inline constexpr std::uint16_t kEnds331[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds332[] = {0u, 2u};
inline constexpr std::uint16_t kEnds333[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds334[] = {0u, 2u};
inline constexpr std::uint16_t kEnds335[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds336[] = {0u, 2u};
inline constexpr std::uint16_t kEnds337[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds338[] = {0u, 2u};
inline constexpr std::uint16_t kEnds339[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds340[] = {0u, 2u};
inline constexpr std::uint16_t kEnds341[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds342[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds343[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds344[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds345[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds346[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds347[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds348[] = {0u, 2u};
inline constexpr std::uint16_t kEnds349[] = {0u, 3u};
inline constexpr std::uint16_t kEnds350[] = {0u, 2u};
inline constexpr std::uint16_t kEnds351[] = {0u, 3u};
inline constexpr std::uint16_t kEnds352[] = {0u, 2u};
inline constexpr std::uint16_t kEnds353[] = {0u, 3u};
inline constexpr std::uint16_t kEnds354[] = {0u, 2u};
inline constexpr std::uint16_t kEnds355[] = {0u, 3u};
inline constexpr std::uint16_t kEnds356[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds357[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds358[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds359[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds360[] = {0u, 2u};
inline constexpr std::uint16_t kEnds361[] = {0u, 3u};
inline constexpr std::uint16_t kEnds362[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds363[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds364[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds365[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds366[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds367[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds368[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds369[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds370[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds371[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds372[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds373[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds374[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds375[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds376[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds377[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds378[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds379[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds380[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds381[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds382[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds383[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds384[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds385[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds386[] = {0u, 2u};
inline constexpr std::uint16_t kEnds387[] = {0u, 3u};
inline constexpr std::uint16_t kEnds388[] = {0u, 2u};
inline constexpr std::uint16_t kEnds389[] = {0u, 3u};
inline constexpr std::uint16_t kEnds390[] = {0u, 2u};
inline constexpr std::uint16_t kEnds391[] = {0u, 3u};
inline constexpr std::uint16_t kEnds392[] = {0u, 2u};
inline constexpr std::uint16_t kEnds393[] = {0u, 3u};
inline constexpr std::uint16_t kEnds394[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds395[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds396[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds397[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds398[] = {0u, 2u};
inline constexpr std::uint16_t kEnds399[] = {0u, 3u};
inline constexpr std::uint16_t kEnds400[] = {0u, 2u};
inline constexpr std::uint16_t kEnds401[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds402[] = {0u, 2u};
inline constexpr std::uint16_t kEnds403[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds404[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds405[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds406[] = {0u, 2u};
inline constexpr std::uint16_t kEnds407[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds408[] = {0u, 2u};
inline constexpr std::uint16_t kEnds409[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds410[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds411[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds412[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds413[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds414[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds415[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds416[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds417[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds418[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds419[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds420[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds421[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds422[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds423[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds424[] = {0u, 2u};
inline constexpr std::uint16_t kEnds425[] = {0u, 3u};
inline constexpr std::uint16_t kEnds426[] = {0u, 2u};
inline constexpr std::uint16_t kEnds427[] = {0u, 3u};
inline constexpr std::uint16_t kEnds428[] = {0u, 2u};
inline constexpr std::uint16_t kEnds429[] = {0u, 3u};
inline constexpr std::uint16_t kEnds430[] = {0u, 2u};
inline constexpr std::uint16_t kEnds431[] = {0u, 3u};
inline constexpr std::uint16_t kEnds432[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds433[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds434[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds435[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds436[] = {0u, 2u};
inline constexpr std::uint16_t kEnds437[] = {0u, 3u};
inline constexpr std::uint16_t kEnds438[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds439[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds440[] = {0u, 2u};
inline constexpr std::uint16_t kEnds441[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds442[] = {0u, 2u};
inline constexpr std::uint16_t kEnds443[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds444[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds445[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds446[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds447[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds448[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds449[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds450[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds451[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds452[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds453[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds454[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds455[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds456[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds457[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds458[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds459[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds460[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds461[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds462[] = {0u, 2u};
inline constexpr std::uint16_t kEnds463[] = {0u, 3u};
inline constexpr std::uint16_t kEnds464[] = {0u, 2u};
inline constexpr std::uint16_t kEnds465[] = {0u, 3u};
inline constexpr std::uint16_t kEnds466[] = {0u, 2u};
inline constexpr std::uint16_t kEnds467[] = {0u, 3u};
inline constexpr std::uint16_t kEnds468[] = {0u, 2u};
inline constexpr std::uint16_t kEnds469[] = {0u, 3u};
inline constexpr std::uint16_t kEnds470[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds471[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds472[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds473[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds474[] = {0u, 2u};
inline constexpr std::uint16_t kEnds475[] = {0u, 3u};
inline constexpr std::uint16_t kEnds476[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds477[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds478[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds479[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds480[] = {0u, 2u};
inline constexpr std::uint16_t kEnds481[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds482[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds483[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds484[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds485[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds486[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds487[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds488[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds489[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds490[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds491[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds492[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds493[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds494[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds495[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds496[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds497[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds498[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds499[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds500[] = {0u, 2u};
inline constexpr std::uint16_t kEnds501[] = {0u, 3u};
inline constexpr std::uint16_t kEnds502[] = {0u, 2u};
inline constexpr std::uint16_t kEnds503[] = {0u, 3u};
inline constexpr std::uint16_t kEnds504[] = {0u, 2u};
inline constexpr std::uint16_t kEnds505[] = {0u, 3u};
inline constexpr std::uint16_t kEnds506[] = {0u, 2u};
inline constexpr std::uint16_t kEnds507[] = {0u, 3u};
inline constexpr std::uint16_t kEnds508[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds509[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds510[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds511[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds512[] = {0u, 2u};
inline constexpr std::uint16_t kEnds513[] = {0u, 3u};
inline constexpr std::uint16_t kEnds514[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds515[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds516[] = {0u, 2u};
inline constexpr std::uint16_t kEnds517[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds518[] = {0u, 2u};
inline constexpr std::uint16_t kEnds519[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds520[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds521[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds522[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds523[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds524[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds525[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds526[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds527[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds528[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds529[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds530[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds531[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds532[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds533[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds534[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds535[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds536[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds537[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds538[] = {0u, 2u};
inline constexpr std::uint16_t kEnds539[] = {0u, 3u};
inline constexpr std::uint16_t kEnds540[] = {0u, 2u};
inline constexpr std::uint16_t kEnds541[] = {0u, 3u};
inline constexpr std::uint16_t kEnds542[] = {0u, 2u};
inline constexpr std::uint16_t kEnds543[] = {0u, 3u};
inline constexpr std::uint16_t kEnds544[] = {0u, 2u};
inline constexpr std::uint16_t kEnds545[] = {0u, 3u};
inline constexpr std::uint16_t kEnds546[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds547[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds548[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds549[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds550[] = {0u, 2u};
inline constexpr std::uint16_t kEnds551[] = {0u, 3u};
inline constexpr std::uint16_t kEnds552[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds553[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds554[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds555[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds556[] = {0u, 2u};
inline constexpr std::uint16_t kEnds557[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds558[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds559[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds560[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds561[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds562[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds563[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds564[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds565[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds566[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds567[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds568[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds569[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds570[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds571[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds572[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds573[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds574[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds575[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds576[] = {0u, 2u};
inline constexpr std::uint16_t kEnds577[] = {0u, 3u};
inline constexpr std::uint16_t kEnds578[] = {0u, 2u};
inline constexpr std::uint16_t kEnds579[] = {0u, 3u};
inline constexpr std::uint16_t kEnds580[] = {0u, 2u};
inline constexpr std::uint16_t kEnds581[] = {0u, 3u};
inline constexpr std::uint16_t kEnds582[] = {0u, 2u};
inline constexpr std::uint16_t kEnds583[] = {0u, 3u};
inline constexpr std::uint16_t kEnds584[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds585[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds586[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds587[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds588[] = {0u, 2u};
inline constexpr std::uint16_t kEnds589[] = {0u, 3u};
inline constexpr std::uint16_t kEnds590[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds591[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds592[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds593[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds594[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds595[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds596[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds597[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds598[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds599[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds600[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds601[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds602[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds603[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds604[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds605[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds606[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds607[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds608[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds609[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds610[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds611[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds612[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds613[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds614[] = {0u, 2u};
inline constexpr std::uint16_t kEnds615[] = {0u, 3u};
inline constexpr std::uint16_t kEnds616[] = {0u, 2u};
inline constexpr std::uint16_t kEnds617[] = {0u, 3u};
inline constexpr std::uint16_t kEnds618[] = {0u, 2u};
inline constexpr std::uint16_t kEnds619[] = {0u, 3u};
inline constexpr std::uint16_t kEnds620[] = {0u, 2u};
inline constexpr std::uint16_t kEnds621[] = {0u, 3u};
inline constexpr std::uint16_t kEnds622[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds623[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds624[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds625[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds626[] = {0u, 2u};
inline constexpr std::uint16_t kEnds627[] = {0u, 3u};
inline constexpr std::uint16_t kEnds628[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds629[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds630[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds631[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds632[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds633[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds634[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds635[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds636[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds637[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds638[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds639[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds640[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds641[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds642[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds643[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds644[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds645[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds646[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds647[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds648[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds649[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds650[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds651[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds652[] = {0u, 2u};
inline constexpr std::uint16_t kEnds653[] = {0u, 3u};
inline constexpr std::uint16_t kEnds654[] = {0u, 2u};
inline constexpr std::uint16_t kEnds655[] = {0u, 3u};
inline constexpr std::uint16_t kEnds656[] = {0u, 2u};
inline constexpr std::uint16_t kEnds657[] = {0u, 3u};
inline constexpr std::uint16_t kEnds658[] = {0u, 2u};
inline constexpr std::uint16_t kEnds659[] = {0u, 3u};
inline constexpr std::uint16_t kEnds660[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds661[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds662[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds663[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds664[] = {0u, 2u};
inline constexpr std::uint16_t kEnds665[] = {0u, 3u};
inline constexpr std::uint16_t kEnds666[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds667[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds668[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds669[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds670[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds671[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds672[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds673[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds674[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds675[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds676[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds677[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds678[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds679[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds680[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds681[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds682[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds683[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds684[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds685[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds686[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds687[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds688[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds689[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds690[] = {0u, 2u};
inline constexpr std::uint16_t kEnds691[] = {0u, 3u};
inline constexpr std::uint16_t kEnds692[] = {0u, 2u};
inline constexpr std::uint16_t kEnds693[] = {0u, 3u};
inline constexpr std::uint16_t kEnds694[] = {0u, 2u};
inline constexpr std::uint16_t kEnds695[] = {0u, 3u};
inline constexpr std::uint16_t kEnds696[] = {0u, 2u};
inline constexpr std::uint16_t kEnds697[] = {0u, 3u};
inline constexpr std::uint16_t kEnds698[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds699[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds700[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds701[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds702[] = {0u, 2u};
inline constexpr std::uint16_t kEnds703[] = {0u, 3u};
inline constexpr std::uint16_t kEnds704[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds705[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds706[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds707[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds708[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds709[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds710[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds711[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds712[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds713[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds714[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds715[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds716[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds717[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds718[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds719[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds720[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds721[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds722[] = {0u, 2u, 3u, 4u, 5u};
inline constexpr std::uint16_t kEnds723[] = {0u, 2u};
inline constexpr std::uint16_t kEnds724[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds725[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds726[] = {0u, 2u};
inline constexpr std::uint16_t kEnds727[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds728[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds729[] = {0u, 4u, 6u, 7u};
inline constexpr std::uint16_t kEnds730[] = {0u, 1u, 5u, 7u, 8u};
inline constexpr std::uint16_t kEnds731[] = {0u, 1u, 6u, 8u, 9u};
inline constexpr std::uint16_t kEnds732[] = {0u, 1u, 4u, 8u, 9u};
inline constexpr std::uint16_t kEnds733[] = {0u, 1u, 5u, 9u, 10u};
inline constexpr std::uint16_t kEnds734[] = {0u, 2u};
inline constexpr std::uint16_t kEnds735[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds736[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds737[] = {0u, 1u, 3u};
inline constexpr std::uint16_t kEnds738[] = {0u, 4u, 6u};
inline constexpr std::uint16_t kEnds739[] = {0u, 3u, 5u};
inline constexpr std::uint16_t kEnds740[] = {0u, 3u, 8u};
inline constexpr std::uint16_t kEnds741[] = {0u, 10u};
inline constexpr std::uint16_t kEnds742[] = {0u, 5u};
inline constexpr std::uint16_t kEnds743[] = {0u, 2u, 4u};
inline constexpr std::uint16_t kEnds744[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds745[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds746[] = {0u, 1u, 2u};
inline constexpr std::uint16_t kEnds747[] = {0u, 3u};
inline constexpr std::uint16_t kEnds748[] = {0u, 4u};
inline constexpr std::uint16_t kEnds749[] = {0u, 4u};
inline constexpr std::uint16_t kEnds750[] = {0u, 5u};
inline constexpr std::uint16_t kEnds751[] = {0u, 5u};
inline constexpr std::uint16_t kEnds752[] = {0u, 5u};
inline constexpr std::uint16_t kEnds753[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds754[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds755[] = {0u, 2u, 3u};
inline constexpr std::uint16_t kEnds756[] = {0u, 4u};
inline constexpr std::uint16_t kEnds757[] = {0u, 5u};
inline constexpr std::uint16_t kEnds758[] = {0u, 3u, 5u};
inline constexpr std::uint16_t kEnds759[] = {0u, 6u};
inline constexpr std::uint16_t kEnds760[] = {0u, 2u, 5u, 10u, 14u};
inline constexpr std::uint16_t kEnds761[] = {0u, 6u};
inline constexpr std::uint16_t kEnds762[] = {0u, 1u, 4u};
inline constexpr std::uint16_t kEnds763[] = {0u, 1u, 4u, 6u};
inline constexpr std::uint16_t kEnds764[] = {0u, 3u, 4u};
inline constexpr std::uint16_t kEnds765[] = {0u, 3u, 5u, 6u};
inline constexpr Case kCases[] = {
    {std::wstring_view(L"\x000D\x000D", 2u), kEnds0},
    {std::wstring_view(L"\x000D\x0308\x000D", 3u), kEnds1},
    {std::wstring_view(L"\x000D\x000A", 2u), kEnds2},
    {std::wstring_view(L"\x000D\x0308\x000A", 3u), kEnds3},
    {std::wstring_view(L"\x000D\x0000", 2u), kEnds4},
    {std::wstring_view(L"\x000D\x0308\x0000", 3u), kEnds5},
    {std::wstring_view(L"\x000D\x094D", 2u), kEnds6},
    {std::wstring_view(L"\x000D\x0308\x094D", 3u), kEnds7},
    {std::wstring_view(L"\x000D\x0300", 2u), kEnds8},
    {std::wstring_view(L"\x000D\x0308\x0300", 3u), kEnds9},
    {std::wstring_view(L"\x000D\x200C", 2u), kEnds10},
    {std::wstring_view(L"\x000D\x0308\x200C", 3u), kEnds11},
    {std::wstring_view(L"\x000D\x200D", 2u), kEnds12},
    {std::wstring_view(L"\x000D\x0308\x200D", 3u), kEnds13},
    {std::wstring_view(L"\x000D\xD83C\xDDE6", 3u), kEnds14},
    {std::wstring_view(L"\x000D\x0308\xD83C\xDDE6", 4u), kEnds15},
    {std::wstring_view(L"\x000D\x06DD", 2u), kEnds16},
    {std::wstring_view(L"\x000D\x0308\x06DD", 3u), kEnds17},
    {std::wstring_view(L"\x000D\x0903", 2u), kEnds18},
    {std::wstring_view(L"\x000D\x0308\x0903", 3u), kEnds19},
    {std::wstring_view(L"\x000D\x1100", 2u), kEnds20},
    {std::wstring_view(L"\x000D\x0308\x1100", 3u), kEnds21},
    {std::wstring_view(L"\x000D\x1160", 2u), kEnds22},
    {std::wstring_view(L"\x000D\x0308\x1160", 3u), kEnds23},
    {std::wstring_view(L"\x000D\x11A8", 2u), kEnds24},
    {std::wstring_view(L"\x000D\x0308\x11A8", 3u), kEnds25},
    {std::wstring_view(L"\x000D\xAC00", 2u), kEnds26},
    {std::wstring_view(L"\x000D\x0308\xAC00", 3u), kEnds27},
    {std::wstring_view(L"\x000D\xAC01", 2u), kEnds28},
    {std::wstring_view(L"\x000D\x0308\xAC01", 3u), kEnds29},
    {std::wstring_view(L"\x000D\x0915", 2u), kEnds30},
    {std::wstring_view(L"\x000D\x0308\x0915", 3u), kEnds31},
    {std::wstring_view(L"\x000D\x00A9", 2u), kEnds32},
    {std::wstring_view(L"\x000D\x0308\x00A9", 3u), kEnds33},
    {std::wstring_view(L"\x000D\x0020", 2u), kEnds34},
    {std::wstring_view(L"\x000D\x0308\x0020", 3u), kEnds35},
    {std::wstring_view(L"\x000D\x0378", 2u), kEnds36},
    {std::wstring_view(L"\x000D\x0308\x0378", 3u), kEnds37},
    {std::wstring_view(L"\x000A\x000D", 2u), kEnds38},
    {std::wstring_view(L"\x000A\x0308\x000D", 3u), kEnds39},
    {std::wstring_view(L"\x000A\x000A", 2u), kEnds40},
    {std::wstring_view(L"\x000A\x0308\x000A", 3u), kEnds41},
    {std::wstring_view(L"\x000A\x0000", 2u), kEnds42},
    {std::wstring_view(L"\x000A\x0308\x0000", 3u), kEnds43},
    {std::wstring_view(L"\x000A\x094D", 2u), kEnds44},
    {std::wstring_view(L"\x000A\x0308\x094D", 3u), kEnds45},
    {std::wstring_view(L"\x000A\x0300", 2u), kEnds46},
    {std::wstring_view(L"\x000A\x0308\x0300", 3u), kEnds47},
    {std::wstring_view(L"\x000A\x200C", 2u), kEnds48},
    {std::wstring_view(L"\x000A\x0308\x200C", 3u), kEnds49},
    {std::wstring_view(L"\x000A\x200D", 2u), kEnds50},
    {std::wstring_view(L"\x000A\x0308\x200D", 3u), kEnds51},
    {std::wstring_view(L"\x000A\xD83C\xDDE6", 3u), kEnds52},
    {std::wstring_view(L"\x000A\x0308\xD83C\xDDE6", 4u), kEnds53},
    {std::wstring_view(L"\x000A\x06DD", 2u), kEnds54},
    {std::wstring_view(L"\x000A\x0308\x06DD", 3u), kEnds55},
    {std::wstring_view(L"\x000A\x0903", 2u), kEnds56},
    {std::wstring_view(L"\x000A\x0308\x0903", 3u), kEnds57},
    {std::wstring_view(L"\x000A\x1100", 2u), kEnds58},
    {std::wstring_view(L"\x000A\x0308\x1100", 3u), kEnds59},
    {std::wstring_view(L"\x000A\x1160", 2u), kEnds60},
    {std::wstring_view(L"\x000A\x0308\x1160", 3u), kEnds61},
    {std::wstring_view(L"\x000A\x11A8", 2u), kEnds62},
    {std::wstring_view(L"\x000A\x0308\x11A8", 3u), kEnds63},
    {std::wstring_view(L"\x000A\xAC00", 2u), kEnds64},
    {std::wstring_view(L"\x000A\x0308\xAC00", 3u), kEnds65},
    {std::wstring_view(L"\x000A\xAC01", 2u), kEnds66},
    {std::wstring_view(L"\x000A\x0308\xAC01", 3u), kEnds67},
    {std::wstring_view(L"\x000A\x0915", 2u), kEnds68},
    {std::wstring_view(L"\x000A\x0308\x0915", 3u), kEnds69},
    {std::wstring_view(L"\x000A\x00A9", 2u), kEnds70},
    {std::wstring_view(L"\x000A\x0308\x00A9", 3u), kEnds71},
    {std::wstring_view(L"\x000A\x0020", 2u), kEnds72},
    {std::wstring_view(L"\x000A\x0308\x0020", 3u), kEnds73},
    {std::wstring_view(L"\x000A\x0378", 2u), kEnds74},
    {std::wstring_view(L"\x000A\x0308\x0378", 3u), kEnds75},
    {std::wstring_view(L"\x0000\x000D", 2u), kEnds76},
    {std::wstring_view(L"\x0000\x0308\x000D", 3u), kEnds77},
    {std::wstring_view(L"\x0000\x000A", 2u), kEnds78},
    {std::wstring_view(L"\x0000\x0308\x000A", 3u), kEnds79},
    {std::wstring_view(L"\x0000\x0000", 2u), kEnds80},
    {std::wstring_view(L"\x0000\x0308\x0000", 3u), kEnds81},
    {std::wstring_view(L"\x0000\x094D", 2u), kEnds82},
    {std::wstring_view(L"\x0000\x0308\x094D", 3u), kEnds83},
    {std::wstring_view(L"\x0000\x0300", 2u), kEnds84},
    {std::wstring_view(L"\x0000\x0308\x0300", 3u), kEnds85},
    {std::wstring_view(L"\x0000\x200C", 2u), kEnds86},
    {std::wstring_view(L"\x0000\x0308\x200C", 3u), kEnds87},
    {std::wstring_view(L"\x0000\x200D", 2u), kEnds88},
    {std::wstring_view(L"\x0000\x0308\x200D", 3u), kEnds89},
    {std::wstring_view(L"\x0000\xD83C\xDDE6", 3u), kEnds90},
    {std::wstring_view(L"\x0000\x0308\xD83C\xDDE6", 4u), kEnds91},
    {std::wstring_view(L"\x0000\x06DD", 2u), kEnds92},
    {std::wstring_view(L"\x0000\x0308\x06DD", 3u), kEnds93},
    {std::wstring_view(L"\x0000\x0903", 2u), kEnds94},
    {std::wstring_view(L"\x0000\x0308\x0903", 3u), kEnds95},
    {std::wstring_view(L"\x0000\x1100", 2u), kEnds96},
    {std::wstring_view(L"\x0000\x0308\x1100", 3u), kEnds97},
    {std::wstring_view(L"\x0000\x1160", 2u), kEnds98},
    {std::wstring_view(L"\x0000\x0308\x1160", 3u), kEnds99},
    {std::wstring_view(L"\x0000\x11A8", 2u), kEnds100},
    {std::wstring_view(L"\x0000\x0308\x11A8", 3u), kEnds101},
    {std::wstring_view(L"\x0000\xAC00", 2u), kEnds102},
    {std::wstring_view(L"\x0000\x0308\xAC00", 3u), kEnds103},
    {std::wstring_view(L"\x0000\xAC01", 2u), kEnds104},
    {std::wstring_view(L"\x0000\x0308\xAC01", 3u), kEnds105},
    {std::wstring_view(L"\x0000\x0915", 2u), kEnds106},
    {std::wstring_view(L"\x0000\x0308\x0915", 3u), kEnds107},
    {std::wstring_view(L"\x0000\x00A9", 2u), kEnds108},
    {std::wstring_view(L"\x0000\x0308\x00A9", 3u), kEnds109},
    {std::wstring_view(L"\x0000\x0020", 2u), kEnds110},
    {std::wstring_view(L"\x0000\x0308\x0020", 3u), kEnds111},
    {std::wstring_view(L"\x0000\x0378", 2u), kEnds112},
    {std::wstring_view(L"\x0000\x0308\x0378", 3u), kEnds113},
    {std::wstring_view(L"\x094D\x000D", 2u), kEnds114},
    {std::wstring_view(L"\x094D\x0308\x000D", 3u), kEnds115},
    {std::wstring_view(L"\x094D\x000A", 2u), kEnds116},
    {std::wstring_view(L"\x094D\x0308\x000A", 3u), kEnds117},
    {std::wstring_view(L"\x094D\x0000", 2u), kEnds118},
    {std::wstring_view(L"\x094D\x0308\x0000", 3u), kEnds119},
    {std::wstring_view(L"\x094D\x094D", 2u), kEnds120},
    {std::wstring_view(L"\x094D\x0308\x094D", 3u), kEnds121},
    {std::wstring_view(L"\x094D\x0300", 2u), kEnds122},
    {std::wstring_view(L"\x094D\x0308\x0300", 3u), kEnds123},
    {std::wstring_view(L"\x094D\x200C", 2u), kEnds124},
    {std::wstring_view(L"\x094D\x0308\x200C", 3u), kEnds125},
    {std::wstring_view(L"\x094D\x200D", 2u), kEnds126},
    {std::wstring_view(L"\x094D\x0308\x200D", 3u), kEnds127},
    {std::wstring_view(L"\x094D\xD83C\xDDE6", 3u), kEnds128},
    {std::wstring_view(L"\x094D\x0308\xD83C\xDDE6", 4u), kEnds129},
    {std::wstring_view(L"\x094D\x06DD", 2u), kEnds130},
    {std::wstring_view(L"\x094D\x0308\x06DD", 3u), kEnds131},
    {std::wstring_view(L"\x094D\x0903", 2u), kEnds132},
    {std::wstring_view(L"\x094D\x0308\x0903", 3u), kEnds133},
    {std::wstring_view(L"\x094D\x1100", 2u), kEnds134},
    {std::wstring_view(L"\x094D\x0308\x1100", 3u), kEnds135},
    {std::wstring_view(L"\x094D\x1160", 2u), kEnds136},
    {std::wstring_view(L"\x094D\x0308\x1160", 3u), kEnds137},
    {std::wstring_view(L"\x094D\x11A8", 2u), kEnds138},
    {std::wstring_view(L"\x094D\x0308\x11A8", 3u), kEnds139},
    {std::wstring_view(L"\x094D\xAC00", 2u), kEnds140},
    {std::wstring_view(L"\x094D\x0308\xAC00", 3u), kEnds141},
    {std::wstring_view(L"\x094D\xAC01", 2u), kEnds142},
    {std::wstring_view(L"\x094D\x0308\xAC01", 3u), kEnds143},
    {std::wstring_view(L"\x094D\x0915", 2u), kEnds144},
    {std::wstring_view(L"\x094D\x0308\x0915", 3u), kEnds145},
    {std::wstring_view(L"\x094D\x00A9", 2u), kEnds146},
    {std::wstring_view(L"\x094D\x0308\x00A9", 3u), kEnds147},
    {std::wstring_view(L"\x094D\x0020", 2u), kEnds148},
    {std::wstring_view(L"\x094D\x0308\x0020", 3u), kEnds149},
    {std::wstring_view(L"\x094D\x0378", 2u), kEnds150},
    {std::wstring_view(L"\x094D\x0308\x0378", 3u), kEnds151},
    {std::wstring_view(L"\x0300\x000D", 2u), kEnds152},
    {std::wstring_view(L"\x0300\x0308\x000D", 3u), kEnds153},
    {std::wstring_view(L"\x0300\x000A", 2u), kEnds154},
    {std::wstring_view(L"\x0300\x0308\x000A", 3u), kEnds155},
    {std::wstring_view(L"\x0300\x0000", 2u), kEnds156},
    {std::wstring_view(L"\x0300\x0308\x0000", 3u), kEnds157},
    {std::wstring_view(L"\x0300\x094D", 2u), kEnds158},
    {std::wstring_view(L"\x0300\x0308\x094D", 3u), kEnds159},
    {std::wstring_view(L"\x0300\x0300", 2u), kEnds160},
    {std::wstring_view(L"\x0300\x0308\x0300", 3u), kEnds161},
    {std::wstring_view(L"\x0300\x200C", 2u), kEnds162},
    {std::wstring_view(L"\x0300\x0308\x200C", 3u), kEnds163},
    {std::wstring_view(L"\x0300\x200D", 2u), kEnds164},
    {std::wstring_view(L"\x0300\x0308\x200D", 3u), kEnds165},
    {std::wstring_view(L"\x0300\xD83C\xDDE6", 3u), kEnds166},
    {std::wstring_view(L"\x0300\x0308\xD83C\xDDE6", 4u), kEnds167},
    {std::wstring_view(L"\x0300\x06DD", 2u), kEnds168},
    {std::wstring_view(L"\x0300\x0308\x06DD", 3u), kEnds169},
    {std::wstring_view(L"\x0300\x0903", 2u), kEnds170},
    {std::wstring_view(L"\x0300\x0308\x0903", 3u), kEnds171},
    {std::wstring_view(L"\x0300\x1100", 2u), kEnds172},
    {std::wstring_view(L"\x0300\x0308\x1100", 3u), kEnds173},
    {std::wstring_view(L"\x0300\x1160", 2u), kEnds174},
    {std::wstring_view(L"\x0300\x0308\x1160", 3u), kEnds175},
    {std::wstring_view(L"\x0300\x11A8", 2u), kEnds176},
    {std::wstring_view(L"\x0300\x0308\x11A8", 3u), kEnds177},
    {std::wstring_view(L"\x0300\xAC00", 2u), kEnds178},
    {std::wstring_view(L"\x0300\x0308\xAC00", 3u), kEnds179},
    {std::wstring_view(L"\x0300\xAC01", 2u), kEnds180},
    {std::wstring_view(L"\x0300\x0308\xAC01", 3u), kEnds181},
    {std::wstring_view(L"\x0300\x0915", 2u), kEnds182},
    {std::wstring_view(L"\x0300\x0308\x0915", 3u), kEnds183},
    {std::wstring_view(L"\x0300\x00A9", 2u), kEnds184},
    {std::wstring_view(L"\x0300\x0308\x00A9", 3u), kEnds185},
    {std::wstring_view(L"\x0300\x0020", 2u), kEnds186},
    {std::wstring_view(L"\x0300\x0308\x0020", 3u), kEnds187},
    {std::wstring_view(L"\x0300\x0378", 2u), kEnds188},
    {std::wstring_view(L"\x0300\x0308\x0378", 3u), kEnds189},
    {std::wstring_view(L"\x200C\x000D", 2u), kEnds190},
    {std::wstring_view(L"\x200C\x0308\x000D", 3u), kEnds191},
    {std::wstring_view(L"\x200C\x000A", 2u), kEnds192},
    {std::wstring_view(L"\x200C\x0308\x000A", 3u), kEnds193},
    {std::wstring_view(L"\x200C\x0000", 2u), kEnds194},
    {std::wstring_view(L"\x200C\x0308\x0000", 3u), kEnds195},
    {std::wstring_view(L"\x200C\x094D", 2u), kEnds196},
    {std::wstring_view(L"\x200C\x0308\x094D", 3u), kEnds197},
    {std::wstring_view(L"\x200C\x0300", 2u), kEnds198},
    {std::wstring_view(L"\x200C\x0308\x0300", 3u), kEnds199},
    {std::wstring_view(L"\x200C\x200C", 2u), kEnds200},
    {std::wstring_view(L"\x200C\x0308\x200C", 3u), kEnds201},
    {std::wstring_view(L"\x200C\x200D", 2u), kEnds202},
    {std::wstring_view(L"\x200C\x0308\x200D", 3u), kEnds203},
    {std::wstring_view(L"\x200C\xD83C\xDDE6", 3u), kEnds204},
    {std::wstring_view(L"\x200C\x0308\xD83C\xDDE6", 4u), kEnds205},
    {std::wstring_view(L"\x200C\x06DD", 2u), kEnds206},
    {std::wstring_view(L"\x200C\x0308\x06DD", 3u), kEnds207},
    {std::wstring_view(L"\x200C\x0903", 2u), kEnds208},
    {std::wstring_view(L"\x200C\x0308\x0903", 3u), kEnds209},
    {std::wstring_view(L"\x200C\x1100", 2u), kEnds210},
    {std::wstring_view(L"\x200C\x0308\x1100", 3u), kEnds211},
    {std::wstring_view(L"\x200C\x1160", 2u), kEnds212},
    {std::wstring_view(L"\x200C\x0308\x1160", 3u), kEnds213},
    {std::wstring_view(L"\x200C\x11A8", 2u), kEnds214},
    {std::wstring_view(L"\x200C\x0308\x11A8", 3u), kEnds215},
    {std::wstring_view(L"\x200C\xAC00", 2u), kEnds216},
    {std::wstring_view(L"\x200C\x0308\xAC00", 3u), kEnds217},
    {std::wstring_view(L"\x200C\xAC01", 2u), kEnds218},
    {std::wstring_view(L"\x200C\x0308\xAC01", 3u), kEnds219},
    {std::wstring_view(L"\x200C\x0915", 2u), kEnds220},
    {std::wstring_view(L"\x200C\x0308\x0915", 3u), kEnds221},
    {std::wstring_view(L"\x200C\x00A9", 2u), kEnds222},
    {std::wstring_view(L"\x200C\x0308\x00A9", 3u), kEnds223},
    {std::wstring_view(L"\x200C\x0020", 2u), kEnds224},
    {std::wstring_view(L"\x200C\x0308\x0020", 3u), kEnds225},
    {std::wstring_view(L"\x200C\x0378", 2u), kEnds226},
    {std::wstring_view(L"\x200C\x0308\x0378", 3u), kEnds227},
    {std::wstring_view(L"\x200D\x000D", 2u), kEnds228},
    {std::wstring_view(L"\x200D\x0308\x000D", 3u), kEnds229},
    {std::wstring_view(L"\x200D\x000A", 2u), kEnds230},
    {std::wstring_view(L"\x200D\x0308\x000A", 3u), kEnds231},
    {std::wstring_view(L"\x200D\x0000", 2u), kEnds232},
    {std::wstring_view(L"\x200D\x0308\x0000", 3u), kEnds233},
    {std::wstring_view(L"\x200D\x094D", 2u), kEnds234},
    {std::wstring_view(L"\x200D\x0308\x094D", 3u), kEnds235},
    {std::wstring_view(L"\x200D\x0300", 2u), kEnds236},
    {std::wstring_view(L"\x200D\x0308\x0300", 3u), kEnds237},
    {std::wstring_view(L"\x200D\x200C", 2u), kEnds238},
    {std::wstring_view(L"\x200D\x0308\x200C", 3u), kEnds239},
    {std::wstring_view(L"\x200D\x200D", 2u), kEnds240},
    {std::wstring_view(L"\x200D\x0308\x200D", 3u), kEnds241},
    {std::wstring_view(L"\x200D\xD83C\xDDE6", 3u), kEnds242},
    {std::wstring_view(L"\x200D\x0308\xD83C\xDDE6", 4u), kEnds243},
    {std::wstring_view(L"\x200D\x06DD", 2u), kEnds244},
    {std::wstring_view(L"\x200D\x0308\x06DD", 3u), kEnds245},
    {std::wstring_view(L"\x200D\x0903", 2u), kEnds246},
    {std::wstring_view(L"\x200D\x0308\x0903", 3u), kEnds247},
    {std::wstring_view(L"\x200D\x1100", 2u), kEnds248},
    {std::wstring_view(L"\x200D\x0308\x1100", 3u), kEnds249},
    {std::wstring_view(L"\x200D\x1160", 2u), kEnds250},
    {std::wstring_view(L"\x200D\x0308\x1160", 3u), kEnds251},
    {std::wstring_view(L"\x200D\x11A8", 2u), kEnds252},
    {std::wstring_view(L"\x200D\x0308\x11A8", 3u), kEnds253},
    {std::wstring_view(L"\x200D\xAC00", 2u), kEnds254},
    {std::wstring_view(L"\x200D\x0308\xAC00", 3u), kEnds255},
    {std::wstring_view(L"\x200D\xAC01", 2u), kEnds256},
    {std::wstring_view(L"\x200D\x0308\xAC01", 3u), kEnds257},
    {std::wstring_view(L"\x200D\x0915", 2u), kEnds258},
    {std::wstring_view(L"\x200D\x0308\x0915", 3u), kEnds259},
    {std::wstring_view(L"\x200D\x00A9", 2u), kEnds260},
    {std::wstring_view(L"\x200D\x0308\x00A9", 3u), kEnds261},
    {std::wstring_view(L"\x200D\x0020", 2u), kEnds262},
    {std::wstring_view(L"\x200D\x0308\x0020", 3u), kEnds263},
    {std::wstring_view(L"\x200D\x0378", 2u), kEnds264},
    {std::wstring_view(L"\x200D\x0308\x0378", 3u), kEnds265},
    {std::wstring_view(L"\xD83C\xDDE6\x000D", 3u), kEnds266},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x000D", 4u), kEnds267},
    {std::wstring_view(L"\xD83C\xDDE6\x000A", 3u), kEnds268},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x000A", 4u), kEnds269},
    {std::wstring_view(L"\xD83C\xDDE6\x0000", 3u), kEnds270},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x0000", 4u), kEnds271},
    {std::wstring_view(L"\xD83C\xDDE6\x094D", 3u), kEnds272},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x094D", 4u), kEnds273},
    {std::wstring_view(L"\xD83C\xDDE6\x0300", 3u), kEnds274},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x0300", 4u), kEnds275},
    {std::wstring_view(L"\xD83C\xDDE6\x200C", 3u), kEnds276},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x200C", 4u), kEnds277},
    {std::wstring_view(L"\xD83C\xDDE6\x200D", 3u), kEnds278},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x200D", 4u), kEnds279},
    {std::wstring_view(L"\xD83C\xDDE6\xD83C\xDDE6", 4u), kEnds280},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\xD83C\xDDE6", 5u), kEnds281},
    {std::wstring_view(L"\xD83C\xDDE6\x06DD", 3u), kEnds282},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x06DD", 4u), kEnds283},
    {std::wstring_view(L"\xD83C\xDDE6\x0903", 3u), kEnds284},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x0903", 4u), kEnds285},
    {std::wstring_view(L"\xD83C\xDDE6\x1100", 3u), kEnds286},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x1100", 4u), kEnds287},
    {std::wstring_view(L"\xD83C\xDDE6\x1160", 3u), kEnds288},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x1160", 4u), kEnds289},
    {std::wstring_view(L"\xD83C\xDDE6\x11A8", 3u), kEnds290},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x11A8", 4u), kEnds291},
    {std::wstring_view(L"\xD83C\xDDE6\xAC00", 3u), kEnds292},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\xAC00", 4u), kEnds293},
    {std::wstring_view(L"\xD83C\xDDE6\xAC01", 3u), kEnds294},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\xAC01", 4u), kEnds295},
    {std::wstring_view(L"\xD83C\xDDE6\x0915", 3u), kEnds296},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x0915", 4u), kEnds297},
    {std::wstring_view(L"\xD83C\xDDE6\x00A9", 3u), kEnds298},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x00A9", 4u), kEnds299},
    {std::wstring_view(L"\xD83C\xDDE6\x0020", 3u), kEnds300},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x0020", 4u), kEnds301},
    {std::wstring_view(L"\xD83C\xDDE6\x0378", 3u), kEnds302},
    {std::wstring_view(L"\xD83C\xDDE6\x0308\x0378", 4u), kEnds303},
    {std::wstring_view(L"\x06DD\x000D", 2u), kEnds304},
    {std::wstring_view(L"\x06DD\x0308\x000D", 3u), kEnds305},
    {std::wstring_view(L"\x06DD\x000A", 2u), kEnds306},
    {std::wstring_view(L"\x06DD\x0308\x000A", 3u), kEnds307},
    {std::wstring_view(L"\x06DD\x0000", 2u), kEnds308},
    {std::wstring_view(L"\x06DD\x0308\x0000", 3u), kEnds309},
    {std::wstring_view(L"\x06DD\x094D", 2u), kEnds310},
    {std::wstring_view(L"\x06DD\x0308\x094D", 3u), kEnds311},
    {std::wstring_view(L"\x06DD\x0300", 2u), kEnds312},
    {std::wstring_view(L"\x06DD\x0308\x0300", 3u), kEnds313},
    {std::wstring_view(L"\x06DD\x200C", 2u), kEnds314},
    {std::wstring_view(L"\x06DD\x0308\x200C", 3u), kEnds315},
    {std::wstring_view(L"\x06DD\x200D", 2u), kEnds316},
    {std::wstring_view(L"\x06DD\x0308\x200D", 3u), kEnds317},
    {std::wstring_view(L"\x06DD\xD83C\xDDE6", 3u), kEnds318},
    {std::wstring_view(L"\x06DD\x0308\xD83C\xDDE6", 4u), kEnds319},
    {std::wstring_view(L"\x06DD\x06DD", 2u), kEnds320},
    {std::wstring_view(L"\x06DD\x0308\x06DD", 3u), kEnds321},
    {std::wstring_view(L"\x06DD\x0903", 2u), kEnds322},
    {std::wstring_view(L"\x06DD\x0308\x0903", 3u), kEnds323},
    {std::wstring_view(L"\x06DD\x1100", 2u), kEnds324},
    {std::wstring_view(L"\x06DD\x0308\x1100", 3u), kEnds325},
    {std::wstring_view(L"\x06DD\x1160", 2u), kEnds326},
    {std::wstring_view(L"\x06DD\x0308\x1160", 3u), kEnds327},
    {std::wstring_view(L"\x06DD\x11A8", 2u), kEnds328},
    {std::wstring_view(L"\x06DD\x0308\x11A8", 3u), kEnds329},
    {std::wstring_view(L"\x06DD\xAC00", 2u), kEnds330},
    {std::wstring_view(L"\x06DD\x0308\xAC00", 3u), kEnds331},
    {std::wstring_view(L"\x06DD\xAC01", 2u), kEnds332},
    {std::wstring_view(L"\x06DD\x0308\xAC01", 3u), kEnds333},
    {std::wstring_view(L"\x06DD\x0915", 2u), kEnds334},
    {std::wstring_view(L"\x06DD\x0308\x0915", 3u), kEnds335},
    {std::wstring_view(L"\x06DD\x00A9", 2u), kEnds336},
    {std::wstring_view(L"\x06DD\x0308\x00A9", 3u), kEnds337},
    {std::wstring_view(L"\x06DD\x0020", 2u), kEnds338},
    {std::wstring_view(L"\x06DD\x0308\x0020", 3u), kEnds339},
    {std::wstring_view(L"\x06DD\x0378", 2u), kEnds340},
    {std::wstring_view(L"\x06DD\x0308\x0378", 3u), kEnds341},
    {std::wstring_view(L"\x0903\x000D", 2u), kEnds342},
    {std::wstring_view(L"\x0903\x0308\x000D", 3u), kEnds343},
    {std::wstring_view(L"\x0903\x000A", 2u), kEnds344},
    {std::wstring_view(L"\x0903\x0308\x000A", 3u), kEnds345},
    {std::wstring_view(L"\x0903\x0000", 2u), kEnds346},
    {std::wstring_view(L"\x0903\x0308\x0000", 3u), kEnds347},
    {std::wstring_view(L"\x0903\x094D", 2u), kEnds348},
    {std::wstring_view(L"\x0903\x0308\x094D", 3u), kEnds349},
    {std::wstring_view(L"\x0903\x0300", 2u), kEnds350},
    {std::wstring_view(L"\x0903\x0308\x0300", 3u), kEnds351},
    {std::wstring_view(L"\x0903\x200C", 2u), kEnds352},
    {std::wstring_view(L"\x0903\x0308\x200C", 3u), kEnds353},
    {std::wstring_view(L"\x0903\x200D", 2u), kEnds354},
    {std::wstring_view(L"\x0903\x0308\x200D", 3u), kEnds355},
    {std::wstring_view(L"\x0903\xD83C\xDDE6", 3u), kEnds356},
    {std::wstring_view(L"\x0903\x0308\xD83C\xDDE6", 4u), kEnds357},
    {std::wstring_view(L"\x0903\x06DD", 2u), kEnds358},
    {std::wstring_view(L"\x0903\x0308\x06DD", 3u), kEnds359},
    {std::wstring_view(L"\x0903\x0903", 2u), kEnds360},
    {std::wstring_view(L"\x0903\x0308\x0903", 3u), kEnds361},
    {std::wstring_view(L"\x0903\x1100", 2u), kEnds362},
    {std::wstring_view(L"\x0903\x0308\x1100", 3u), kEnds363},
    {std::wstring_view(L"\x0903\x1160", 2u), kEnds364},
    {std::wstring_view(L"\x0903\x0308\x1160", 3u), kEnds365},
    {std::wstring_view(L"\x0903\x11A8", 2u), kEnds366},
    {std::wstring_view(L"\x0903\x0308\x11A8", 3u), kEnds367},
    {std::wstring_view(L"\x0903\xAC00", 2u), kEnds368},
    {std::wstring_view(L"\x0903\x0308\xAC00", 3u), kEnds369},
    {std::wstring_view(L"\x0903\xAC01", 2u), kEnds370},
    {std::wstring_view(L"\x0903\x0308\xAC01", 3u), kEnds371},
    {std::wstring_view(L"\x0903\x0915", 2u), kEnds372},
    {std::wstring_view(L"\x0903\x0308\x0915", 3u), kEnds373},
    {std::wstring_view(L"\x0903\x00A9", 2u), kEnds374},
    {std::wstring_view(L"\x0903\x0308\x00A9", 3u), kEnds375},
    {std::wstring_view(L"\x0903\x0020", 2u), kEnds376},
    {std::wstring_view(L"\x0903\x0308\x0020", 3u), kEnds377},
    {std::wstring_view(L"\x0903\x0378", 2u), kEnds378},
    {std::wstring_view(L"\x0903\x0308\x0378", 3u), kEnds379},
    {std::wstring_view(L"\x1100\x000D", 2u), kEnds380},
    {std::wstring_view(L"\x1100\x0308\x000D", 3u), kEnds381},
    {std::wstring_view(L"\x1100\x000A", 2u), kEnds382},
    {std::wstring_view(L"\x1100\x0308\x000A", 3u), kEnds383},
    {std::wstring_view(L"\x1100\x0000", 2u), kEnds384},
    {std::wstring_view(L"\x1100\x0308\x0000", 3u), kEnds385},
    {std::wstring_view(L"\x1100\x094D", 2u), kEnds386},
    {std::wstring_view(L"\x1100\x0308\x094D", 3u), kEnds387},
    {std::wstring_view(L"\x1100\x0300", 2u), kEnds388},
    {std::wstring_view(L"\x1100\x0308\x0300", 3u), kEnds389},
    {std::wstring_view(L"\x1100\x200C", 2u), kEnds390},
    {std::wstring_view(L"\x1100\x0308\x200C", 3u), kEnds391},
    {std::wstring_view(L"\x1100\x200D", 2u), kEnds392},
    {std::wstring_view(L"\x1100\x0308\x200D", 3u), kEnds393},
    {std::wstring_view(L"\x1100\xD83C\xDDE6", 3u), kEnds394},
    {std::wstring_view(L"\x1100\x0308\xD83C\xDDE6", 4u), kEnds395},
    {std::wstring_view(L"\x1100\x06DD", 2u), kEnds396},
    {std::wstring_view(L"\x1100\x0308\x06DD", 3u), kEnds397},
    {std::wstring_view(L"\x1100\x0903", 2u), kEnds398},
    {std::wstring_view(L"\x1100\x0308\x0903", 3u), kEnds399},
    {std::wstring_view(L"\x1100\x1100", 2u), kEnds400},
    {std::wstring_view(L"\x1100\x0308\x1100", 3u), kEnds401},
    {std::wstring_view(L"\x1100\x1160", 2u), kEnds402},
    {std::wstring_view(L"\x1100\x0308\x1160", 3u), kEnds403},
    {std::wstring_view(L"\x1100\x11A8", 2u), kEnds404},
    {std::wstring_view(L"\x1100\x0308\x11A8", 3u), kEnds405},
    {std::wstring_view(L"\x1100\xAC00", 2u), kEnds406},
    {std::wstring_view(L"\x1100\x0308\xAC00", 3u), kEnds407},
    {std::wstring_view(L"\x1100\xAC01", 2u), kEnds408},
    {std::wstring_view(L"\x1100\x0308\xAC01", 3u), kEnds409},
    {std::wstring_view(L"\x1100\x0915", 2u), kEnds410},
    {std::wstring_view(L"\x1100\x0308\x0915", 3u), kEnds411},
    {std::wstring_view(L"\x1100\x00A9", 2u), kEnds412},
    {std::wstring_view(L"\x1100\x0308\x00A9", 3u), kEnds413},
    {std::wstring_view(L"\x1100\x0020", 2u), kEnds414},
    {std::wstring_view(L"\x1100\x0308\x0020", 3u), kEnds415},
    {std::wstring_view(L"\x1100\x0378", 2u), kEnds416},
    {std::wstring_view(L"\x1100\x0308\x0378", 3u), kEnds417},
    {std::wstring_view(L"\x1160\x000D", 2u), kEnds418},
    {std::wstring_view(L"\x1160\x0308\x000D", 3u), kEnds419},
    {std::wstring_view(L"\x1160\x000A", 2u), kEnds420},
    {std::wstring_view(L"\x1160\x0308\x000A", 3u), kEnds421},
    {std::wstring_view(L"\x1160\x0000", 2u), kEnds422},
    {std::wstring_view(L"\x1160\x0308\x0000", 3u), kEnds423},
    {std::wstring_view(L"\x1160\x094D", 2u), kEnds424},
    {std::wstring_view(L"\x1160\x0308\x094D", 3u), kEnds425},
    {std::wstring_view(L"\x1160\x0300", 2u), kEnds426},
    {std::wstring_view(L"\x1160\x0308\x0300", 3u), kEnds427},
    {std::wstring_view(L"\x1160\x200C", 2u), kEnds428},
    {std::wstring_view(L"\x1160\x0308\x200C", 3u), kEnds429},
    {std::wstring_view(L"\x1160\x200D", 2u), kEnds430},
    {std::wstring_view(L"\x1160\x0308\x200D", 3u), kEnds431},
    {std::wstring_view(L"\x1160\xD83C\xDDE6", 3u), kEnds432},
    {std::wstring_view(L"\x1160\x0308\xD83C\xDDE6", 4u), kEnds433},
    {std::wstring_view(L"\x1160\x06DD", 2u), kEnds434},
    {std::wstring_view(L"\x1160\x0308\x06DD", 3u), kEnds435},
    {std::wstring_view(L"\x1160\x0903", 2u), kEnds436},
    {std::wstring_view(L"\x1160\x0308\x0903", 3u), kEnds437},
    {std::wstring_view(L"\x1160\x1100", 2u), kEnds438},
    {std::wstring_view(L"\x1160\x0308\x1100", 3u), kEnds439},
    {std::wstring_view(L"\x1160\x1160", 2u), kEnds440},
    {std::wstring_view(L"\x1160\x0308\x1160", 3u), kEnds441},
    {std::wstring_view(L"\x1160\x11A8", 2u), kEnds442},
    {std::wstring_view(L"\x1160\x0308\x11A8", 3u), kEnds443},
    {std::wstring_view(L"\x1160\xAC00", 2u), kEnds444},
    {std::wstring_view(L"\x1160\x0308\xAC00", 3u), kEnds445},
    {std::wstring_view(L"\x1160\xAC01", 2u), kEnds446},
    {std::wstring_view(L"\x1160\x0308\xAC01", 3u), kEnds447},
    {std::wstring_view(L"\x1160\x0915", 2u), kEnds448},
    {std::wstring_view(L"\x1160\x0308\x0915", 3u), kEnds449},
    {std::wstring_view(L"\x1160\x00A9", 2u), kEnds450},
    {std::wstring_view(L"\x1160\x0308\x00A9", 3u), kEnds451},
    {std::wstring_view(L"\x1160\x0020", 2u), kEnds452},
    {std::wstring_view(L"\x1160\x0308\x0020", 3u), kEnds453},
    {std::wstring_view(L"\x1160\x0378", 2u), kEnds454},
    {std::wstring_view(L"\x1160\x0308\x0378", 3u), kEnds455},
    {std::wstring_view(L"\x11A8\x000D", 2u), kEnds456},
    {std::wstring_view(L"\x11A8\x0308\x000D", 3u), kEnds457},
    {std::wstring_view(L"\x11A8\x000A", 2u), kEnds458},
    {std::wstring_view(L"\x11A8\x0308\x000A", 3u), kEnds459},
    {std::wstring_view(L"\x11A8\x0000", 2u), kEnds460},
    {std::wstring_view(L"\x11A8\x0308\x0000", 3u), kEnds461},
    {std::wstring_view(L"\x11A8\x094D", 2u), kEnds462},
    {std::wstring_view(L"\x11A8\x0308\x094D", 3u), kEnds463},
    {std::wstring_view(L"\x11A8\x0300", 2u), kEnds464},
    {std::wstring_view(L"\x11A8\x0308\x0300", 3u), kEnds465},
    {std::wstring_view(L"\x11A8\x200C", 2u), kEnds466},
    {std::wstring_view(L"\x11A8\x0308\x200C", 3u), kEnds467},
    {std::wstring_view(L"\x11A8\x200D", 2u), kEnds468},
    {std::wstring_view(L"\x11A8\x0308\x200D", 3u), kEnds469},
    {std::wstring_view(L"\x11A8\xD83C\xDDE6", 3u), kEnds470},
    {std::wstring_view(L"\x11A8\x0308\xD83C\xDDE6", 4u), kEnds471},
    {std::wstring_view(L"\x11A8\x06DD", 2u), kEnds472},
    {std::wstring_view(L"\x11A8\x0308\x06DD", 3u), kEnds473},
    {std::wstring_view(L"\x11A8\x0903", 2u), kEnds474},
    {std::wstring_view(L"\x11A8\x0308\x0903", 3u), kEnds475},
    {std::wstring_view(L"\x11A8\x1100", 2u), kEnds476},
    {std::wstring_view(L"\x11A8\x0308\x1100", 3u), kEnds477},
    {std::wstring_view(L"\x11A8\x1160", 2u), kEnds478},
    {std::wstring_view(L"\x11A8\x0308\x1160", 3u), kEnds479},
    {std::wstring_view(L"\x11A8\x11A8", 2u), kEnds480},
    {std::wstring_view(L"\x11A8\x0308\x11A8", 3u), kEnds481},
    {std::wstring_view(L"\x11A8\xAC00", 2u), kEnds482},
    {std::wstring_view(L"\x11A8\x0308\xAC00", 3u), kEnds483},
    {std::wstring_view(L"\x11A8\xAC01", 2u), kEnds484},
    {std::wstring_view(L"\x11A8\x0308\xAC01", 3u), kEnds485},
    {std::wstring_view(L"\x11A8\x0915", 2u), kEnds486},
    {std::wstring_view(L"\x11A8\x0308\x0915", 3u), kEnds487},
    {std::wstring_view(L"\x11A8\x00A9", 2u), kEnds488},
    {std::wstring_view(L"\x11A8\x0308\x00A9", 3u), kEnds489},
    {std::wstring_view(L"\x11A8\x0020", 2u), kEnds490},
    {std::wstring_view(L"\x11A8\x0308\x0020", 3u), kEnds491},
    {std::wstring_view(L"\x11A8\x0378", 2u), kEnds492},
    {std::wstring_view(L"\x11A8\x0308\x0378", 3u), kEnds493},
    {std::wstring_view(L"\xAC00\x000D", 2u), kEnds494},
    {std::wstring_view(L"\xAC00\x0308\x000D", 3u), kEnds495},
    {std::wstring_view(L"\xAC00\x000A", 2u), kEnds496},
    {std::wstring_view(L"\xAC00\x0308\x000A", 3u), kEnds497},
    {std::wstring_view(L"\xAC00\x0000", 2u), kEnds498},
    {std::wstring_view(L"\xAC00\x0308\x0000", 3u), kEnds499},
    {std::wstring_view(L"\xAC00\x094D", 2u), kEnds500},
    {std::wstring_view(L"\xAC00\x0308\x094D", 3u), kEnds501},
    {std::wstring_view(L"\xAC00\x0300", 2u), kEnds502},
    {std::wstring_view(L"\xAC00\x0308\x0300", 3u), kEnds503},
    {std::wstring_view(L"\xAC00\x200C", 2u), kEnds504},
    {std::wstring_view(L"\xAC00\x0308\x200C", 3u), kEnds505},
    {std::wstring_view(L"\xAC00\x200D", 2u), kEnds506},
    {std::wstring_view(L"\xAC00\x0308\x200D", 3u), kEnds507},
    {std::wstring_view(L"\xAC00\xD83C\xDDE6", 3u), kEnds508},
    {std::wstring_view(L"\xAC00\x0308\xD83C\xDDE6", 4u), kEnds509},
    {std::wstring_view(L"\xAC00\x06DD", 2u), kEnds510},
    {std::wstring_view(L"\xAC00\x0308\x06DD", 3u), kEnds511},
    {std::wstring_view(L"\xAC00\x0903", 2u), kEnds512},
    {std::wstring_view(L"\xAC00\x0308\x0903", 3u), kEnds513},
    {std::wstring_view(L"\xAC00\x1100", 2u), kEnds514},
    {std::wstring_view(L"\xAC00\x0308\x1100", 3u), kEnds515},
    {std::wstring_view(L"\xAC00\x1160", 2u), kEnds516},
    {std::wstring_view(L"\xAC00\x0308\x1160", 3u), kEnds517},
    {std::wstring_view(L"\xAC00\x11A8", 2u), kEnds518},
    {std::wstring_view(L"\xAC00\x0308\x11A8", 3u), kEnds519},
    {std::wstring_view(L"\xAC00\xAC00", 2u), kEnds520},
    {std::wstring_view(L"\xAC00\x0308\xAC00", 3u), kEnds521},
    {std::wstring_view(L"\xAC00\xAC01", 2u), kEnds522},
    {std::wstring_view(L"\xAC00\x0308\xAC01", 3u), kEnds523},
    {std::wstring_view(L"\xAC00\x0915", 2u), kEnds524},
    {std::wstring_view(L"\xAC00\x0308\x0915", 3u), kEnds525},
    {std::wstring_view(L"\xAC00\x00A9", 2u), kEnds526},
    {std::wstring_view(L"\xAC00\x0308\x00A9", 3u), kEnds527},
    {std::wstring_view(L"\xAC00\x0020", 2u), kEnds528},
    {std::wstring_view(L"\xAC00\x0308\x0020", 3u), kEnds529},
    {std::wstring_view(L"\xAC00\x0378", 2u), kEnds530},
    {std::wstring_view(L"\xAC00\x0308\x0378", 3u), kEnds531},
    {std::wstring_view(L"\xAC01\x000D", 2u), kEnds532},
    {std::wstring_view(L"\xAC01\x0308\x000D", 3u), kEnds533},
    {std::wstring_view(L"\xAC01\x000A", 2u), kEnds534},
    {std::wstring_view(L"\xAC01\x0308\x000A", 3u), kEnds535},
    {std::wstring_view(L"\xAC01\x0000", 2u), kEnds536},
    {std::wstring_view(L"\xAC01\x0308\x0000", 3u), kEnds537},
    {std::wstring_view(L"\xAC01\x094D", 2u), kEnds538},
    {std::wstring_view(L"\xAC01\x0308\x094D", 3u), kEnds539},
    {std::wstring_view(L"\xAC01\x0300", 2u), kEnds540},
    {std::wstring_view(L"\xAC01\x0308\x0300", 3u), kEnds541},
    {std::wstring_view(L"\xAC01\x200C", 2u), kEnds542},
    {std::wstring_view(L"\xAC01\x0308\x200C", 3u), kEnds543},
    {std::wstring_view(L"\xAC01\x200D", 2u), kEnds544},
    {std::wstring_view(L"\xAC01\x0308\x200D", 3u), kEnds545},
    {std::wstring_view(L"\xAC01\xD83C\xDDE6", 3u), kEnds546},
    {std::wstring_view(L"\xAC01\x0308\xD83C\xDDE6", 4u), kEnds547},
    {std::wstring_view(L"\xAC01\x06DD", 2u), kEnds548},
    {std::wstring_view(L"\xAC01\x0308\x06DD", 3u), kEnds549},
    {std::wstring_view(L"\xAC01\x0903", 2u), kEnds550},
    {std::wstring_view(L"\xAC01\x0308\x0903", 3u), kEnds551},
    {std::wstring_view(L"\xAC01\x1100", 2u), kEnds552},
    {std::wstring_view(L"\xAC01\x0308\x1100", 3u), kEnds553},
    {std::wstring_view(L"\xAC01\x1160", 2u), kEnds554},
    {std::wstring_view(L"\xAC01\x0308\x1160", 3u), kEnds555},
    {std::wstring_view(L"\xAC01\x11A8", 2u), kEnds556},
    {std::wstring_view(L"\xAC01\x0308\x11A8", 3u), kEnds557},
    {std::wstring_view(L"\xAC01\xAC00", 2u), kEnds558},
    {std::wstring_view(L"\xAC01\x0308\xAC00", 3u), kEnds559},
    {std::wstring_view(L"\xAC01\xAC01", 2u), kEnds560},
    {std::wstring_view(L"\xAC01\x0308\xAC01", 3u), kEnds561},
    {std::wstring_view(L"\xAC01\x0915", 2u), kEnds562},
    {std::wstring_view(L"\xAC01\x0308\x0915", 3u), kEnds563},
    {std::wstring_view(L"\xAC01\x00A9", 2u), kEnds564},
    {std::wstring_view(L"\xAC01\x0308\x00A9", 3u), kEnds565},
    {std::wstring_view(L"\xAC01\x0020", 2u), kEnds566},
    {std::wstring_view(L"\xAC01\x0308\x0020", 3u), kEnds567},
    {std::wstring_view(L"\xAC01\x0378", 2u), kEnds568},
    {std::wstring_view(L"\xAC01\x0308\x0378", 3u), kEnds569},
    {std::wstring_view(L"\x0915\x000D", 2u), kEnds570},
    {std::wstring_view(L"\x0915\x0308\x000D", 3u), kEnds571},
    {std::wstring_view(L"\x0915\x000A", 2u), kEnds572},
    {std::wstring_view(L"\x0915\x0308\x000A", 3u), kEnds573},
    {std::wstring_view(L"\x0915\x0000", 2u), kEnds574},
    {std::wstring_view(L"\x0915\x0308\x0000", 3u), kEnds575},
    {std::wstring_view(L"\x0915\x094D", 2u), kEnds576},
    {std::wstring_view(L"\x0915\x0308\x094D", 3u), kEnds577},
    {std::wstring_view(L"\x0915\x0300", 2u), kEnds578},
    {std::wstring_view(L"\x0915\x0308\x0300", 3u), kEnds579},
    {std::wstring_view(L"\x0915\x200C", 2u), kEnds580},
    {std::wstring_view(L"\x0915\x0308\x200C", 3u), kEnds581},
    {std::wstring_view(L"\x0915\x200D", 2u), kEnds582},
    {std::wstring_view(L"\x0915\x0308\x200D", 3u), kEnds583},
    {std::wstring_view(L"\x0915\xD83C\xDDE6", 3u), kEnds584},
    {std::wstring_view(L"\x0915\x0308\xD83C\xDDE6", 4u), kEnds585},
    {std::wstring_view(L"\x0915\x06DD", 2u), kEnds586},
    {std::wstring_view(L"\x0915\x0308\x06DD", 3u), kEnds587},
    {std::wstring_view(L"\x0915\x0903", 2u), kEnds588},
    {std::wstring_view(L"\x0915\x0308\x0903", 3u), kEnds589},
    {std::wstring_view(L"\x0915\x1100", 2u), kEnds590},
    {std::wstring_view(L"\x0915\x0308\x1100", 3u), kEnds591},
    {std::wstring_view(L"\x0915\x1160", 2u), kEnds592},
    {std::wstring_view(L"\x0915\x0308\x1160", 3u), kEnds593},
    {std::wstring_view(L"\x0915\x11A8", 2u), kEnds594},
    {std::wstring_view(L"\x0915\x0308\x11A8", 3u), kEnds595},
    {std::wstring_view(L"\x0915\xAC00", 2u), kEnds596},
    {std::wstring_view(L"\x0915\x0308\xAC00", 3u), kEnds597},
    {std::wstring_view(L"\x0915\xAC01", 2u), kEnds598},
    {std::wstring_view(L"\x0915\x0308\xAC01", 3u), kEnds599},
    {std::wstring_view(L"\x0915\x0915", 2u), kEnds600},
    {std::wstring_view(L"\x0915\x0308\x0915", 3u), kEnds601},
    {std::wstring_view(L"\x0915\x00A9", 2u), kEnds602},
    {std::wstring_view(L"\x0915\x0308\x00A9", 3u), kEnds603},
    {std::wstring_view(L"\x0915\x0020", 2u), kEnds604},
    {std::wstring_view(L"\x0915\x0308\x0020", 3u), kEnds605},
    {std::wstring_view(L"\x0915\x0378", 2u), kEnds606},
    {std::wstring_view(L"\x0915\x0308\x0378", 3u), kEnds607},
    {std::wstring_view(L"\x00A9\x000D", 2u), kEnds608},
    {std::wstring_view(L"\x00A9\x0308\x000D", 3u), kEnds609},
    {std::wstring_view(L"\x00A9\x000A", 2u), kEnds610},
    {std::wstring_view(L"\x00A9\x0308\x000A", 3u), kEnds611},
    {std::wstring_view(L"\x00A9\x0000", 2u), kEnds612},
    {std::wstring_view(L"\x00A9\x0308\x0000", 3u), kEnds613},
    {std::wstring_view(L"\x00A9\x094D", 2u), kEnds614},
    {std::wstring_view(L"\x00A9\x0308\x094D", 3u), kEnds615},
    {std::wstring_view(L"\x00A9\x0300", 2u), kEnds616},
    {std::wstring_view(L"\x00A9\x0308\x0300", 3u), kEnds617},
    {std::wstring_view(L"\x00A9\x200C", 2u), kEnds618},
    {std::wstring_view(L"\x00A9\x0308\x200C", 3u), kEnds619},
    {std::wstring_view(L"\x00A9\x200D", 2u), kEnds620},
    {std::wstring_view(L"\x00A9\x0308\x200D", 3u), kEnds621},
    {std::wstring_view(L"\x00A9\xD83C\xDDE6", 3u), kEnds622},
    {std::wstring_view(L"\x00A9\x0308\xD83C\xDDE6", 4u), kEnds623},
    {std::wstring_view(L"\x00A9\x06DD", 2u), kEnds624},
    {std::wstring_view(L"\x00A9\x0308\x06DD", 3u), kEnds625},
    {std::wstring_view(L"\x00A9\x0903", 2u), kEnds626},
    {std::wstring_view(L"\x00A9\x0308\x0903", 3u), kEnds627},
    {std::wstring_view(L"\x00A9\x1100", 2u), kEnds628},
    {std::wstring_view(L"\x00A9\x0308\x1100", 3u), kEnds629},
    {std::wstring_view(L"\x00A9\x1160", 2u), kEnds630},
    {std::wstring_view(L"\x00A9\x0308\x1160", 3u), kEnds631},
    {std::wstring_view(L"\x00A9\x11A8", 2u), kEnds632},
    {std::wstring_view(L"\x00A9\x0308\x11A8", 3u), kEnds633},
    {std::wstring_view(L"\x00A9\xAC00", 2u), kEnds634},
    {std::wstring_view(L"\x00A9\x0308\xAC00", 3u), kEnds635},
    {std::wstring_view(L"\x00A9\xAC01", 2u), kEnds636},
    {std::wstring_view(L"\x00A9\x0308\xAC01", 3u), kEnds637},
    {std::wstring_view(L"\x00A9\x0915", 2u), kEnds638},
    {std::wstring_view(L"\x00A9\x0308\x0915", 3u), kEnds639},
    {std::wstring_view(L"\x00A9\x00A9", 2u), kEnds640},
    {std::wstring_view(L"\x00A9\x0308\x00A9", 3u), kEnds641},
    {std::wstring_view(L"\x00A9\x0020", 2u), kEnds642},
    {std::wstring_view(L"\x00A9\x0308\x0020", 3u), kEnds643},
    {std::wstring_view(L"\x00A9\x0378", 2u), kEnds644},
    {std::wstring_view(L"\x00A9\x0308\x0378", 3u), kEnds645},
    {std::wstring_view(L"\x0020\x000D", 2u), kEnds646},
    {std::wstring_view(L"\x0020\x0308\x000D", 3u), kEnds647},
    {std::wstring_view(L"\x0020\x000A", 2u), kEnds648},
    {std::wstring_view(L"\x0020\x0308\x000A", 3u), kEnds649},
    {std::wstring_view(L"\x0020\x0000", 2u), kEnds650},
    {std::wstring_view(L"\x0020\x0308\x0000", 3u), kEnds651},
    {std::wstring_view(L"\x0020\x094D", 2u), kEnds652},
    {std::wstring_view(L"\x0020\x0308\x094D", 3u), kEnds653},
    {std::wstring_view(L"\x0020\x0300", 2u), kEnds654},
    {std::wstring_view(L"\x0020\x0308\x0300", 3u), kEnds655},
    {std::wstring_view(L"\x0020\x200C", 2u), kEnds656},
    {std::wstring_view(L"\x0020\x0308\x200C", 3u), kEnds657},
    {std::wstring_view(L"\x0020\x200D", 2u), kEnds658},
    {std::wstring_view(L"\x0020\x0308\x200D", 3u), kEnds659},
    {std::wstring_view(L"\x0020\xD83C\xDDE6", 3u), kEnds660},
    {std::wstring_view(L"\x0020\x0308\xD83C\xDDE6", 4u), kEnds661},
    {std::wstring_view(L"\x0020\x06DD", 2u), kEnds662},
    {std::wstring_view(L"\x0020\x0308\x06DD", 3u), kEnds663},
    {std::wstring_view(L"\x0020\x0903", 2u), kEnds664},
    {std::wstring_view(L"\x0020\x0308\x0903", 3u), kEnds665},
    {std::wstring_view(L"\x0020\x1100", 2u), kEnds666},
    {std::wstring_view(L"\x0020\x0308\x1100", 3u), kEnds667},
    {std::wstring_view(L"\x0020\x1160", 2u), kEnds668},
    {std::wstring_view(L"\x0020\x0308\x1160", 3u), kEnds669},
    {std::wstring_view(L"\x0020\x11A8", 2u), kEnds670},
    {std::wstring_view(L"\x0020\x0308\x11A8", 3u), kEnds671},
    {std::wstring_view(L"\x0020\xAC00", 2u), kEnds672},
    {std::wstring_view(L"\x0020\x0308\xAC00", 3u), kEnds673},
    {std::wstring_view(L"\x0020\xAC01", 2u), kEnds674},
    {std::wstring_view(L"\x0020\x0308\xAC01", 3u), kEnds675},
    {std::wstring_view(L"\x0020\x0915", 2u), kEnds676},
    {std::wstring_view(L"\x0020\x0308\x0915", 3u), kEnds677},
    {std::wstring_view(L"\x0020\x00A9", 2u), kEnds678},
    {std::wstring_view(L"\x0020\x0308\x00A9", 3u), kEnds679},
    {std::wstring_view(L"\x0020\x0020", 2u), kEnds680},
    {std::wstring_view(L"\x0020\x0308\x0020", 3u), kEnds681},
    {std::wstring_view(L"\x0020\x0378", 2u), kEnds682},
    {std::wstring_view(L"\x0020\x0308\x0378", 3u), kEnds683},
    {std::wstring_view(L"\x0378\x000D", 2u), kEnds684},
    {std::wstring_view(L"\x0378\x0308\x000D", 3u), kEnds685},
    {std::wstring_view(L"\x0378\x000A", 2u), kEnds686},
    {std::wstring_view(L"\x0378\x0308\x000A", 3u), kEnds687},
    {std::wstring_view(L"\x0378\x0000", 2u), kEnds688},
    {std::wstring_view(L"\x0378\x0308\x0000", 3u), kEnds689},
    {std::wstring_view(L"\x0378\x094D", 2u), kEnds690},
    {std::wstring_view(L"\x0378\x0308\x094D", 3u), kEnds691},
    {std::wstring_view(L"\x0378\x0300", 2u), kEnds692},
    {std::wstring_view(L"\x0378\x0308\x0300", 3u), kEnds693},
    {std::wstring_view(L"\x0378\x200C", 2u), kEnds694},
    {std::wstring_view(L"\x0378\x0308\x200C", 3u), kEnds695},
    {std::wstring_view(L"\x0378\x200D", 2u), kEnds696},
    {std::wstring_view(L"\x0378\x0308\x200D", 3u), kEnds697},
    {std::wstring_view(L"\x0378\xD83C\xDDE6", 3u), kEnds698},
    {std::wstring_view(L"\x0378\x0308\xD83C\xDDE6", 4u), kEnds699},
    {std::wstring_view(L"\x0378\x06DD", 2u), kEnds700},
    {std::wstring_view(L"\x0378\x0308\x06DD", 3u), kEnds701},
    {std::wstring_view(L"\x0378\x0903", 2u), kEnds702},
    {std::wstring_view(L"\x0378\x0308\x0903", 3u), kEnds703},
    {std::wstring_view(L"\x0378\x1100", 2u), kEnds704},
    {std::wstring_view(L"\x0378\x0308\x1100", 3u), kEnds705},
    {std::wstring_view(L"\x0378\x1160", 2u), kEnds706},
    {std::wstring_view(L"\x0378\x0308\x1160", 3u), kEnds707},
    {std::wstring_view(L"\x0378\x11A8", 2u), kEnds708},
    {std::wstring_view(L"\x0378\x0308\x11A8", 3u), kEnds709},
    {std::wstring_view(L"\x0378\xAC00", 2u), kEnds710},
    {std::wstring_view(L"\x0378\x0308\xAC00", 3u), kEnds711},
    {std::wstring_view(L"\x0378\xAC01", 2u), kEnds712},
    {std::wstring_view(L"\x0378\x0308\xAC01", 3u), kEnds713},
    {std::wstring_view(L"\x0378\x0915", 2u), kEnds714},
    {std::wstring_view(L"\x0378\x0308\x0915", 3u), kEnds715},
    {std::wstring_view(L"\x0378\x00A9", 2u), kEnds716},
    {std::wstring_view(L"\x0378\x0308\x00A9", 3u), kEnds717},
    {std::wstring_view(L"\x0378\x0020", 2u), kEnds718},
    {std::wstring_view(L"\x0378\x0308\x0020", 3u), kEnds719},
    {std::wstring_view(L"\x0378\x0378", 2u), kEnds720},
    {std::wstring_view(L"\x0378\x0308\x0378", 3u), kEnds721},
    {std::wstring_view(L"\x000D\x000A\x0061\x000A\x0308", 5u), kEnds722},
    {std::wstring_view(L"\x0061\x0308", 2u), kEnds723},
    {std::wstring_view(L"\x0020\x200D\x0646", 3u), kEnds724},
    {std::wstring_view(L"\x0646\x200D\x0020", 3u), kEnds725},
    {std::wstring_view(L"\x1100\x1100", 2u), kEnds726},
    {std::wstring_view(L"\xAC00\x11A8\x1100", 3u), kEnds727},
    {std::wstring_view(L"\xAC01\x11A8\x1100", 3u), kEnds728},
    {std::wstring_view(L"\xD83C\xDDE6\xD83C\xDDE7\xD83C\xDDE8\x0062", 7u), kEnds729},
    {std::wstring_view(L"\x0061\xD83C\xDDE6\xD83C\xDDE7\xD83C\xDDE8\x0062", 8u), kEnds730},
    {std::wstring_view(L"\x0061\xD83C\xDDE6\xD83C\xDDE7\x200D\xD83C\xDDE8\x0062", 9u), kEnds731},
    {std::wstring_view(L"\x0061\xD83C\xDDE6\x200D\xD83C\xDDE7\xD83C\xDDE8\x0062", 9u), kEnds732},
    {std::wstring_view(L"\x0061\xD83C\xDDE6\xD83C\xDDE7\xD83C\xDDE8\xD83C\xDDE9\x0062", 10u), kEnds733},
    {std::wstring_view(L"\x0061\x200D", 2u), kEnds734},
    {std::wstring_view(L"\x0061\x0308\x0062", 3u), kEnds735},
    {std::wstring_view(L"\x0061\x0903\x0062", 3u), kEnds736},
    {std::wstring_view(L"\x0061\x0600\x0062", 3u), kEnds737},
    {std::wstring_view(L"\xD83D\xDC76\xD83C\xDFFF\xD83D\xDC76", 6u), kEnds738},
    {std::wstring_view(L"\x0061\xD83C\xDFFF\xD83D\xDC76", 5u), kEnds739},
    {std::wstring_view(L"\x0061\xD83C\xDFFF\xD83D\xDC76\x200D\xD83D\xDED1", 8u), kEnds740},
    {std::wstring_view(L"\xD83D\xDC76\xD83C\xDFFF\x0308\x200D\xD83D\xDC76\xD83C\xDFFF", 10u), kEnds741},
    {std::wstring_view(L"\xD83D\xDED1\x200D\xD83D\xDED1", 5u), kEnds742},
    {std::wstring_view(L"\x0061\x200D\xD83D\xDED1", 4u), kEnds743},
    {std::wstring_view(L"\x2701\x200D\x2701", 3u), kEnds744},
    {std::wstring_view(L"\x0061\x200D\x2701", 3u), kEnds745},
    {std::wstring_view(L"\x0915\x0924", 2u), kEnds746},
    {std::wstring_view(L"\x0915\x094D\x0924", 3u), kEnds747},
    {std::wstring_view(L"\x0915\x094D\x094D\x0924", 4u), kEnds748},
    {std::wstring_view(L"\x0915\x094D\x200D\x0924", 4u), kEnds749},
    {std::wstring_view(L"\x0915\x093C\x200D\x094D\x0924", 5u), kEnds750},
    {std::wstring_view(L"\x0915\x093C\x094D\x200D\x0924", 5u), kEnds751},
    {std::wstring_view(L"\x0915\x094D\x0924\x094D\x092F", 5u), kEnds752},
    {std::wstring_view(L"\x0915\x094D\x0061", 3u), kEnds753},
    {std::wstring_view(L"\x0061\x094D\x0924", 3u), kEnds754},
    {std::wstring_view(L"\x003F\x094D\x0924", 3u), kEnds755},
    {std::wstring_view(L"\x0915\x094D\x094D\x0924", 4u), kEnds756},
    {std::wstring_view(L"\x0AB8\x0AFB\x0ACD\x0AB8\x0AFB", 5u), kEnds757},
    {std::wstring_view(L"\x1019\x1039\x1018\x102C\x1037", 5u), kEnds758},
    {std::wstring_view(L"\x1004\x103A\x1039\x1011\x1039\x1011", 6u), kEnds759},
    {std::wstring_view(L"\x1B12\x1B01\x1B32\x1B44\x1B2F\x1B32\x1B44\x1B22\x1B44\x1B2C\x1B32\x1B44\x1B22\x1B38", 14u), kEnds760},
    {std::wstring_view(L"\x179F\x17D2\x178F\x17D2\x179A\x17B8", 6u), kEnds761},
    {std::wstring_view(L"\x1B26\x1B17\x1B44\x1B13", 4u), kEnds762},
    {std::wstring_view(L"\x1B27\x1B13\x1B44\x1B0B\x1B0B\x1B04", 6u), kEnds763},
    {std::wstring_view(L"\x1795\x17D2\x17AF\x1798", 4u), kEnds764},
    {std::wstring_view(L"\x17A0\x17D2\x17AB\x1791\x17D0\x1799", 6u), kEnds765},
};
// clang-format on
} // namespace UnicodeGraphemeCorpus
