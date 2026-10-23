/**
  ******************************************************************************
  * @file    har7class_v6_int8.c
  * @author  AST Embedded Analytics Research Platform
  * @date    2026-05-27T19:40:42+0800
  * @brief   AI Tool Automatic Code Generator for Embedded NN computing
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  ******************************************************************************
  */


#include "har7class_v6_int8.h"
#include "har7class_v6_int8_data.h"

#include "ai_platform.h"
#include "ai_platform_interface.h"
#include "ai_math_helpers.h"

#include "core_common.h"
#include "core_convert.h"

#include "layers.h"



#undef AI_NET_OBJ_INSTANCE
#define AI_NET_OBJ_INSTANCE g_har7class_v6_int8
 
#undef AI_HAR7CLASS_V6_INT8_MODEL_SIGNATURE
#define AI_HAR7CLASS_V6_INT8_MODEL_SIGNATURE     "0x4b172998a9566a9db52d4e961066bf2c"

#ifndef AI_TOOLS_REVISION_ID
#define AI_TOOLS_REVISION_ID     ""
#endif

#undef AI_TOOLS_DATE_TIME
#define AI_TOOLS_DATE_TIME   "2026-05-27T19:40:42+0800"

#undef AI_TOOLS_COMPILE_TIME
#define AI_TOOLS_COMPILE_TIME    __DATE__ " " __TIME__

#undef AI_HAR7CLASS_V6_INT8_N_BATCHES
#define AI_HAR7CLASS_V6_INT8_N_BATCHES         (1)

static ai_ptr g_har7class_v6_int8_activations_map[1] = AI_C_ARRAY_INIT;
static ai_ptr g_har7class_v6_int8_weights_map[1] = AI_C_ARRAY_INIT;



/**  Array declarations section  **********************************************/
/* Array#0 */
AI_ARRAY_OBJ_DECLARE(
  imu_output_array, AI_ARRAY_FORMAT_U8|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 3072, AI_STATIC)

/* Array#1 */
AI_ARRAY_OBJ_DECLARE(
  imu_Transpose_output_array, AI_ARRAY_FORMAT_U8,
  NULL, NULL, 3072, AI_STATIC)

/* Array#2 */
AI_ARRAY_OBJ_DECLARE(
  relu_output_array, AI_ARRAY_FORMAT_U8,
  NULL, NULL, 16384, AI_STATIC)

/* Array#3 */
AI_ARRAY_OBJ_DECLARE(
  relu_1_output_array, AI_ARRAY_FORMAT_U8,
  NULL, NULL, 8192, AI_STATIC)

/* Array#4 */
AI_ARRAY_OBJ_DECLARE(
  relu_2_output_array, AI_ARRAY_FORMAT_U8,
  NULL, NULL, 16384, AI_STATIC)

/* Array#5 */
AI_ARRAY_OBJ_DECLARE(
  relu_3_output_array, AI_ARRAY_FORMAT_U8,
  NULL, NULL, 8192, AI_STATIC)

/* Array#6 */
AI_ARRAY_OBJ_DECLARE(
  relu_4_output_array, AI_ARRAY_FORMAT_U8,
  NULL, NULL, 16384, AI_STATIC)

/* Array#7 */
AI_ARRAY_OBJ_DECLARE(
  relu_4_0_0_mean_conversion_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 16384, AI_STATIC)

/* Array#8 */
AI_ARRAY_OBJ_DECLARE(
  mean_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#9 */
AI_ARRAY_OBJ_DECLARE(
  mean_Mul_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#10 */
AI_ARRAY_OBJ_DECLARE(
  relu_5_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#11 */
AI_ARRAY_OBJ_DECLARE(
  logits_QuantizeLinear_Input_output_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 7, AI_STATIC)

/* Array#12 */
AI_ARRAY_OBJ_DECLARE(
  logits_QuantizeLinear_Input_0_conversion_output_array, AI_ARRAY_FORMAT_U8|AI_FMT_FLAG_IS_IO,
  NULL, NULL, 7, AI_STATIC)

/* Array#13 */
AI_ARRAY_OBJ_DECLARE(
  relu_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 960, AI_STATIC)

/* Array#14 */
AI_ARRAY_OBJ_DECLARE(
  relu_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 32, AI_STATIC)

/* Array#15 */
AI_ARRAY_OBJ_DECLARE(
  relu_1_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 5120, AI_STATIC)

/* Array#16 */
AI_ARRAY_OBJ_DECLARE(
  relu_1_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 32, AI_STATIC)

/* Array#17 */
AI_ARRAY_OBJ_DECLARE(
  relu_2_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6144, AI_STATIC)

/* Array#18 */
AI_ARRAY_OBJ_DECLARE(
  relu_2_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 64, AI_STATIC)

/* Array#19 */
AI_ARRAY_OBJ_DECLARE(
  relu_3_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 12288, AI_STATIC)

/* Array#20 */
AI_ARRAY_OBJ_DECLARE(
  relu_3_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 64, AI_STATIC)

/* Array#21 */
AI_ARRAY_OBJ_DECLARE(
  relu_4_weights_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 24576, AI_STATIC)

/* Array#22 */
AI_ARRAY_OBJ_DECLARE(
  relu_4_bias_array, AI_ARRAY_FORMAT_S32,
  NULL, NULL, 128, AI_STATIC)

/* Array#23 */
AI_ARRAY_OBJ_DECLARE(
  mean_Mul_scale_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#24 */
AI_ARRAY_OBJ_DECLARE(
  mean_Mul_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 128, AI_STATIC)

/* Array#25 */
AI_ARRAY_OBJ_DECLARE(
  relu_5_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 8192, AI_STATIC)

/* Array#26 */
AI_ARRAY_OBJ_DECLARE(
  relu_5_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 64, AI_STATIC)

/* Array#27 */
AI_ARRAY_OBJ_DECLARE(
  logits_QuantizeLinear_Input_weights_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 448, AI_STATIC)

/* Array#28 */
AI_ARRAY_OBJ_DECLARE(
  logits_QuantizeLinear_Input_bias_array, AI_ARRAY_FORMAT_FLOAT,
  NULL, NULL, 7, AI_STATIC)

/* Array#29 */
AI_ARRAY_OBJ_DECLARE(
  relu_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 2488, AI_STATIC)

/* Array#30 */
AI_ARRAY_OBJ_DECLARE(
  relu_1_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6208, AI_STATIC)

/* Array#31 */
AI_ARRAY_OBJ_DECLARE(
  relu_1_scratch1_array, AI_ARRAY_FORMAT_U8,
  NULL, NULL, 64, AI_STATIC)

/* Array#32 */
AI_ARRAY_OBJ_DECLARE(
  relu_2_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6400, AI_STATIC)

/* Array#33 */
AI_ARRAY_OBJ_DECLARE(
  relu_3_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 6784, AI_STATIC)

/* Array#34 */
AI_ARRAY_OBJ_DECLARE(
  relu_3_scratch1_array, AI_ARRAY_FORMAT_U8,
  NULL, NULL, 128, AI_STATIC)

/* Array#35 */
AI_ARRAY_OBJ_DECLARE(
  relu_4_scratch0_array, AI_ARRAY_FORMAT_S8,
  NULL, NULL, 7680, AI_STATIC)

/**  Array metadata declarations section  *************************************/
/* Int quant #0 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(imu_Transpose_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.16555362939834595f),
    AI_PACK_UINTQ_ZP(122)))

/* Int quant #1 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(imu_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.16555362939834595f),
    AI_PACK_UINTQ_ZP(122)))

/* Int quant #2 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(logits_QuantizeLinear_Input_0_conversion_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.08877704292535782f),
    AI_PACK_UINTQ_ZP(132)))

/* Int quant #3 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_1_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.13563500344753265f),
    AI_PACK_UINTQ_ZP(0)))

/* Int quant #4 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_1_scratch1_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.13563500344753265f),
    AI_PACK_UINTQ_ZP(0)))

/* Int quant #5 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_1_weights_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 32,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0010193911148235202f, 0.0024030504282563925f, 0.0009217170299962163f, 0.0011964505538344383f, 0.002208220772445202f, 0.0011910642497241497f, 0.002030827570706606f, 0.00409160740673542f, 0.001904047792777419f, 0.0017815217142924666f, 0.000993552734144032f, 0.0011449138401076198f, 0.0012459661811590195f, 0.0015672934241592884f, 0.0012899517314508557f, 0.0009282873943448067f, 0.001311738626100123f, 0.0015280924271792173f, 0.0011333097936585546f, 0.0020747848320752382f, 0.0017657720018178225f, 0.0008824196993373334f, 0.0009142246562987566f, 0.0014544902369379997f, 0.0026763540226966143f, 0.001512338174507022f, 0.001197732170112431f, 0.0011315412120893598f, 0.0010199053213000298f, 0.002854114631190896f, 0.001545687671750784f, 0.0009935051202774048f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #6 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_2_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.09768187254667282f),
    AI_PACK_UINTQ_ZP(0)))

/* Int quant #7 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_2_weights_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 64,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0031893306877464056f, 0.0014583718730136752f, 0.0024904559832066298f, 0.0018282214878126979f, 0.0016140452353283763f, 0.0018752445466816425f, 0.0026009075809270144f, 0.0020841313526034355f, 0.002462750533595681f, 0.0030352200847119093f, 0.0021740251686424017f, 0.002486457582563162f, 0.002476363442838192f, 0.0035126733127981424f, 0.0037891999818384647f, 0.0029646086040884256f, 0.002103479579091072f, 0.0017845322145149112f, 0.0019407690269872546f, 0.0028527951799333096f, 0.0013281147694215178f, 0.004239679779857397f, 0.001983104972168803f, 0.0018191528506577015f, 0.0026857133489102125f, 0.0020852743182331324f, 0.0025216219946742058f, 0.00289128627628088f, 0.0023969283793121576f, 0.0031541294883936644f, 0.0022377055138349533f, 0.002300023566931486f, 0.002600085688754916f, 0.003947414457798004f, 0.002871364587917924f, 0.003218409838154912f, 0.0025694495998322964f, 0.0024075370747596025f, 0.0021818107925355434f, 0.0026094450149685144f, 0.003120575100183487f, 0.0028193718753755093f, 0.0022218029480427504f, 0.003875682596117258f, 0.004053314682096243f, 0.002605271525681019f, 0.0027345409616827965f, 0.0029067264404147863f, 0.002564032329246402f, 0.0027881732676178217f, 0.0035509783774614334f, 0.0022089064586907625f, 0.0026213808450847864f, 0.003138128435239196f, 0.0028531651478260756f, 0.003935388755053282f, 0.0015390334883704782f, 0.0018904650351032615f, 0.0024769625160843134f, 0.0012698856880888343f, 0.0030770613811910152f, 0.0031016806606203318f, 0.0017836588667705655f, 0.0051597715355455875f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #8 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_3_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.034376706928014755f),
    AI_PACK_UINTQ_ZP(0)))

/* Int quant #9 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_3_scratch1_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.034376706928014755f),
    AI_PACK_UINTQ_ZP(0)))

/* Int quant #10 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_3_weights_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 64,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0013207091251388192f, 0.002684461884200573f, 0.00283884909003973f, 0.0012033351231366396f, 0.001165850437246263f, 0.0014741607010364532f, 0.0014080206165090203f, 0.0015831615310162306f, 0.0018310927553102374f, 0.001062398892827332f, 0.0013181122485548258f, 0.0024679210036993027f, 0.0013416848378255963f, 0.0013852663105353713f, 0.0013048314722254872f, 0.0013515809550881386f, 0.002292528050020337f, 0.0015480067813768983f, 0.0013900345657020807f, 0.0018517635762691498f, 0.0014784015947952867f, 0.0016364771872758865f, 0.0010900082997977734f, 0.0013482128269970417f, 0.001276754424907267f, 0.0010360449086874723f, 0.0025851086247712374f, 0.0017647040076553822f, 0.0011927238665521145f, 0.0017977611860260367f, 0.001649808487854898f, 0.0020912757609039545f, 0.0012350579490885139f, 0.0010392251424491405f, 0.0017724900972098112f, 0.0012226803228259087f, 0.0025244203861802816f, 0.00243916898034513f, 0.00178785331081599f, 0.0013367229839786887f, 0.001971398713067174f, 0.0013200141256675124f, 0.0011550303315743804f, 0.001524497289210558f, 0.002841297071427107f, 0.0018118483712896705f, 0.002250773599371314f, 0.0012533164117485285f, 0.0012201005592942238f, 0.002649070229381323f, 0.0010288120247423649f, 0.0013805212220177054f, 0.00105983205139637f, 0.0021818799432367086f, 0.0014216938288882375f, 0.001142660970799625f, 0.0014844308607280254f, 0.0018417479004710913f, 0.002357274992391467f, 0.0016920823836699128f, 0.0019391296664252877f, 0.0012242099037393928f, 0.0014839043142274022f, 0.001546006416901946f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #11 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_4_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.03507557138800621f),
    AI_PACK_UINTQ_ZP(0)))

/* Int quant #12 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_4_weights_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 128,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0029688612557947636f, 0.0014646531781181693f, 0.001149857183918357f, 0.0013410384999588132f, 0.0016470446716994047f, 0.0020232517272233963f, 0.001218853984028101f, 0.0014131305506452918f, 0.002432876033708453f, 0.0023505338467657566f, 0.0016558913048356771f, 0.0014312154380604625f, 0.0018390279728919268f, 0.0015318976948037744f, 0.001450582523830235f, 0.0010937594342976809f, 0.0017694445559754968f, 0.0013804257614538074f, 0.001548871980048716f, 0.0014928120654076338f, 0.00195802329108119f, 0.0014206436462700367f, 0.0010773144895210862f, 0.0016642133705317974f, 0.0016474153380841017f, 0.0016915672458708286f, 0.0017999426927417517f, 0.0012646943796426058f, 0.0015453482046723366f, 0.00131162628531456f, 0.002205511089414358f, 0.001533815753646195f, 0.001358281122520566f, 0.002037080703303218f, 0.0016233341302722692f, 0.00212602480314672f, 0.0023121123667806387f, 0.0009651822037994862f, 0.002277514198794961f, 0.0012514898553490639f, 0.00155472825281322f, 0.0013895350275561213f, 0.0018989065429195762f, 0.0019895844161510468f, 0.0024910562206059694f, 0.0015667399857193232f, 0.0015229866839945316f, 0.001523718936368823f, 0.0015266145346686244f, 0.0017490361351519823f, 0.002043394837528467f, 0.0014046141877770424f, 0.0030493936501443386f, 0.001315161818638444f, 0.0018371536862105131f, 0.0017257463186979294f, 0.001299227587878704f, 0.002023102482780814f, 0.001494803698733449f, 0.0015969438245519996f, 0.001757427933625877f, 0.0015052391681820154f, 0.001144066103734076f, 0.0013223224086686969f, 0.0014807728584855795f, 0.0014172621304169297f, 0.0024227683898061514f, 0.001876905793324113f, 0.0011967107420787215f, 0.0017274023266509175f, 0.0018020293209701777f, 0.0015161593910306692f, 0.0015691667795181274f, 0.0014356186147779226f, 0.0017705087084323168f, 0.0019381982274353504f, 0.0015728630824014544f, 0.0014769306872040033f, 0.0016866229707375169f, 0.002002533059567213f, 0.0016650118632242084f, 0.002253779675811529f, 0.002011155476793647f, 0.0018378511304035783f, 0.0016967988340184093f, 0.0019476052839308977f, 0.0016665986040607095f, 0.0015054942341521382f, 0.0016175549244508147f, 0.0017433430766686797f, 0.00181805610191077f, 0.0021571458783000708f, 0.001481056329794228f, 0.002748906146734953f, 0.0012085712514817715f, 0.00120434514246881f, 0.0018331976607441902f, 0.001222850289195776f, 0.0022279894910752773f, 0.0012606628006324172f, 0.0019873627461493015f, 0.001862568547949195f, 0.0017052022740244865f, 0.0017456546192988753f, 0.0015136107103899121f, 0.0013380612945184112f, 0.0017168845515698195f, 0.001835277071222663f, 0.0016655687941238284f, 0.0013044537045061588f, 0.001924922806210816f, 0.001388339907862246f, 0.0012907807249575853f, 0.0016564856050536036f, 0.001544767408631742f, 0.0015151631087064743f, 0.0012031624792143703f, 0.0013021411141380668f, 0.002011997625231743f, 0.001380541711114347f, 0.001749717746861279f, 0.001586355036124587f, 0.0015775287756696343f, 0.0019360965816304088f, 0.0024685896933078766f, 0.001519734738394618f, 0.0014986295718699694f, 0.0016582140233367682f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/* Int quant #13 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_output_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_U8, 1,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.14510022103786469f),
    AI_PACK_UINTQ_ZP(0)))

/* Int quant #14 */
AI_INTQ_INFO_LIST_OBJ_DECLARE(relu_weights_array_intq, AI_STATIC_CONST,
  AI_BUFFER_META_FLAG_SCALE_FLOAT|AI_BUFFER_META_FLAG_ZEROPOINT_S8, 32,
  AI_PACK_INTQ_INFO(
    AI_PACK_INTQ_SCALE(0.0019086637767031789f, 0.0013447355013340712f, 0.003717568004503846f, 0.004024958703666925f, 0.001884679077193141f, 0.002037455327808857f, 0.0031876638531684875f, 0.0024151746183633804f, 0.0020311621483415365f, 0.003134580096229911f, 0.004276295658200979f, 0.0018997235456481576f, 0.0015149255050346255f, 0.0021812093909829855f, 0.0023540365509688854f, 0.002188826911151409f, 0.0019796905107796192f, 0.003748744959011674f, 0.0023903867695480585f, 0.0022488918621093035f, 0.0022973762825131416f, 0.0024178880266845226f, 0.002650480018928647f, 0.0022412247490137815f, 0.009681027382612228f, 0.004107012879103422f, 0.0025096647441387177f, 0.0044641075655817986f, 0.0022958782501518726f, 0.0052944631315767765f, 0.0015214411541819572f, 0.0031497080344706774f),
    AI_PACK_INTQ_ZP(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)))

/**  Tensor declarations section  *********************************************/
/* Tensor #0 */
AI_TENSOR_OBJ_DECLARE(
  imu_Transpose_output, AI_STATIC,
  0, 0x1,
  AI_SHAPE_INIT(4, 1, 6, 1, 512), AI_STRIDE_INIT(4, 1, 1, 6, 6),
  1, &imu_Transpose_output_array, &imu_Transpose_output_array_intq)

/* Tensor #1 */
AI_TENSOR_OBJ_DECLARE(
  imu_output, AI_STATIC,
  1, 0x1,
  AI_SHAPE_INIT(4, 1, 512, 1, 6), AI_STRIDE_INIT(4, 1, 1, 512, 512),
  1, &imu_output_array, &imu_output_array_intq)

/* Tensor #2 */
AI_TENSOR_OBJ_DECLARE(
  logits_QuantizeLinear_Input_0_conversion_output, AI_STATIC,
  2, 0x1,
  AI_SHAPE_INIT(4, 1, 7, 1, 1), AI_STRIDE_INIT(4, 1, 1, 7, 7),
  1, &logits_QuantizeLinear_Input_0_conversion_output_array, &logits_QuantizeLinear_Input_0_conversion_output_array_intq)

/* Tensor #3 */
AI_TENSOR_OBJ_DECLARE(
  logits_QuantizeLinear_Input_bias, AI_STATIC,
  3, 0x0,
  AI_SHAPE_INIT(4, 1, 7, 1, 1), AI_STRIDE_INIT(4, 4, 4, 28, 28),
  1, &logits_QuantizeLinear_Input_bias_array, NULL)

/* Tensor #4 */
AI_TENSOR_OBJ_DECLARE(
  logits_QuantizeLinear_Input_output, AI_STATIC,
  4, 0x0,
  AI_SHAPE_INIT(4, 1, 7, 1, 1), AI_STRIDE_INIT(4, 4, 4, 28, 28),
  1, &logits_QuantizeLinear_Input_output_array, NULL)

/* Tensor #5 */
AI_TENSOR_OBJ_DECLARE(
  logits_QuantizeLinear_Input_weights, AI_STATIC,
  5, 0x0,
  AI_SHAPE_INIT(4, 64, 7, 1, 1), AI_STRIDE_INIT(4, 4, 256, 1792, 1792),
  1, &logits_QuantizeLinear_Input_weights_array, NULL)

/* Tensor #6 */
AI_TENSOR_OBJ_DECLARE(
  mean_Mul_bias, AI_STATIC,
  6, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &mean_Mul_bias_array, NULL)

/* Tensor #7 */
AI_TENSOR_OBJ_DECLARE(
  mean_Mul_output, AI_STATIC,
  7, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &mean_Mul_output_array, NULL)

/* Tensor #8 */
AI_TENSOR_OBJ_DECLARE(
  mean_Mul_scale, AI_STATIC,
  8, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &mean_Mul_scale_array, NULL)

/* Tensor #9 */
AI_TENSOR_OBJ_DECLARE(
  mean_output, AI_STATIC,
  9, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &mean_output_array, NULL)

/* Tensor #10 */
AI_TENSOR_OBJ_DECLARE(
  relu_1_bias, AI_STATIC,
  10, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 1, 1), AI_STRIDE_INIT(4, 4, 4, 128, 128),
  1, &relu_1_bias_array, NULL)

/* Tensor #11 */
AI_TENSOR_OBJ_DECLARE(
  relu_1_output, AI_STATIC,
  11, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 1, 256), AI_STRIDE_INIT(4, 1, 1, 32, 32),
  1, &relu_1_output_array, &relu_1_output_array_intq)

/* Tensor #12 */
AI_TENSOR_OBJ_DECLARE(
  relu_1_scratch0, AI_STATIC,
  12, 0x0,
  AI_SHAPE_INIT(4, 1, 6208, 1, 1), AI_STRIDE_INIT(4, 1, 1, 6208, 6208),
  1, &relu_1_scratch0_array, NULL)

/* Tensor #13 */
AI_TENSOR_OBJ_DECLARE(
  relu_1_scratch1, AI_STATIC,
  13, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 1, 2), AI_STRIDE_INIT(4, 1, 1, 32, 32),
  1, &relu_1_scratch1_array, &relu_1_scratch1_array_intq)

/* Tensor #14 */
AI_TENSOR_OBJ_DECLARE(
  relu_1_weights, AI_STATIC,
  14, 0x1,
  AI_SHAPE_INIT(4, 32, 1, 5, 32), AI_STRIDE_INIT(4, 1, 32, 1024, 1024),
  1, &relu_1_weights_array, &relu_1_weights_array_intq)

/* Tensor #15 */
AI_TENSOR_OBJ_DECLARE(
  relu_2_bias, AI_STATIC,
  15, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &relu_2_bias_array, NULL)

/* Tensor #16 */
AI_TENSOR_OBJ_DECLARE(
  relu_2_output, AI_STATIC,
  16, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 256), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &relu_2_output_array, &relu_2_output_array_intq)

/* Tensor #17 */
AI_TENSOR_OBJ_DECLARE(
  relu_2_scratch0, AI_STATIC,
  17, 0x0,
  AI_SHAPE_INIT(4, 1, 6400, 1, 1), AI_STRIDE_INIT(4, 1, 1, 6400, 6400),
  1, &relu_2_scratch0_array, NULL)

/* Tensor #18 */
AI_TENSOR_OBJ_DECLARE(
  relu_2_weights, AI_STATIC,
  18, 0x1,
  AI_SHAPE_INIT(4, 32, 1, 3, 64), AI_STRIDE_INIT(4, 1, 32, 2048, 2048),
  1, &relu_2_weights_array, &relu_2_weights_array_intq)

/* Tensor #19 */
AI_TENSOR_OBJ_DECLARE(
  relu_3_bias, AI_STATIC,
  19, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &relu_3_bias_array, NULL)

/* Tensor #20 */
AI_TENSOR_OBJ_DECLARE(
  relu_3_output, AI_STATIC,
  20, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 128), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &relu_3_output_array, &relu_3_output_array_intq)

/* Tensor #21 */
AI_TENSOR_OBJ_DECLARE(
  relu_3_scratch0, AI_STATIC,
  21, 0x0,
  AI_SHAPE_INIT(4, 1, 6784, 1, 1), AI_STRIDE_INIT(4, 1, 1, 6784, 6784),
  1, &relu_3_scratch0_array, NULL)

/* Tensor #22 */
AI_TENSOR_OBJ_DECLARE(
  relu_3_scratch1, AI_STATIC,
  22, 0x1,
  AI_SHAPE_INIT(4, 1, 64, 1, 2), AI_STRIDE_INIT(4, 1, 1, 64, 64),
  1, &relu_3_scratch1_array, &relu_3_scratch1_array_intq)

/* Tensor #23 */
AI_TENSOR_OBJ_DECLARE(
  relu_3_weights, AI_STATIC,
  23, 0x1,
  AI_SHAPE_INIT(4, 64, 1, 3, 64), AI_STRIDE_INIT(4, 1, 64, 4096, 4096),
  1, &relu_3_weights_array, &relu_3_weights_array_intq)

/* Tensor #24 */
AI_TENSOR_OBJ_DECLARE(
  relu_4_0_0_mean_conversion_output, AI_STATIC,
  24, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 128), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &relu_4_0_0_mean_conversion_output_array, NULL)

/* Tensor #25 */
AI_TENSOR_OBJ_DECLARE(
  relu_4_0_0_mean_conversion_output0, AI_STATIC,
  25, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 128, 1), AI_STRIDE_INIT(4, 4, 4, 512, 65536),
  1, &relu_4_0_0_mean_conversion_output_array, NULL)

/* Tensor #26 */
AI_TENSOR_OBJ_DECLARE(
  relu_4_bias, AI_STATIC,
  26, 0x0,
  AI_SHAPE_INIT(4, 1, 128, 1, 1), AI_STRIDE_INIT(4, 4, 4, 512, 512),
  1, &relu_4_bias_array, NULL)

/* Tensor #27 */
AI_TENSOR_OBJ_DECLARE(
  relu_4_output, AI_STATIC,
  27, 0x1,
  AI_SHAPE_INIT(4, 1, 128, 1, 128), AI_STRIDE_INIT(4, 1, 1, 128, 128),
  1, &relu_4_output_array, &relu_4_output_array_intq)

/* Tensor #28 */
AI_TENSOR_OBJ_DECLARE(
  relu_4_scratch0, AI_STATIC,
  28, 0x0,
  AI_SHAPE_INIT(4, 1, 7680, 1, 1), AI_STRIDE_INIT(4, 1, 1, 7680, 7680),
  1, &relu_4_scratch0_array, NULL)

/* Tensor #29 */
AI_TENSOR_OBJ_DECLARE(
  relu_4_weights, AI_STATIC,
  29, 0x1,
  AI_SHAPE_INIT(4, 64, 1, 3, 128), AI_STRIDE_INIT(4, 1, 64, 8192, 8192),
  1, &relu_4_weights_array, &relu_4_weights_array_intq)

/* Tensor #30 */
AI_TENSOR_OBJ_DECLARE(
  relu_5_bias, AI_STATIC,
  30, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &relu_5_bias_array, NULL)

/* Tensor #31 */
AI_TENSOR_OBJ_DECLARE(
  relu_5_output, AI_STATIC,
  31, 0x0,
  AI_SHAPE_INIT(4, 1, 64, 1, 1), AI_STRIDE_INIT(4, 4, 4, 256, 256),
  1, &relu_5_output_array, NULL)

/* Tensor #32 */
AI_TENSOR_OBJ_DECLARE(
  relu_5_weights, AI_STATIC,
  32, 0x0,
  AI_SHAPE_INIT(4, 128, 64, 1, 1), AI_STRIDE_INIT(4, 4, 512, 32768, 32768),
  1, &relu_5_weights_array, NULL)

/* Tensor #33 */
AI_TENSOR_OBJ_DECLARE(
  relu_bias, AI_STATIC,
  33, 0x0,
  AI_SHAPE_INIT(4, 1, 32, 1, 1), AI_STRIDE_INIT(4, 4, 4, 128, 128),
  1, &relu_bias_array, NULL)

/* Tensor #34 */
AI_TENSOR_OBJ_DECLARE(
  relu_output, AI_STATIC,
  34, 0x1,
  AI_SHAPE_INIT(4, 1, 32, 1, 512), AI_STRIDE_INIT(4, 1, 1, 32, 32),
  1, &relu_output_array, &relu_output_array_intq)

/* Tensor #35 */
AI_TENSOR_OBJ_DECLARE(
  relu_scratch0, AI_STATIC,
  35, 0x0,
  AI_SHAPE_INIT(4, 1, 2488, 1, 1), AI_STRIDE_INIT(4, 1, 1, 2488, 2488),
  1, &relu_scratch0_array, NULL)

/* Tensor #36 */
AI_TENSOR_OBJ_DECLARE(
  relu_weights, AI_STATIC,
  36, 0x1,
  AI_SHAPE_INIT(4, 6, 1, 5, 32), AI_STRIDE_INIT(4, 1, 6, 192, 192),
  1, &relu_weights_array, &relu_weights_array_intq)



/**  Layer declarations section  **********************************************/


AI_TENSOR_CHAIN_OBJ_DECLARE(
  logits_QuantizeLinear_Input_0_conversion_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &logits_QuantizeLinear_Input_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &logits_QuantizeLinear_Input_0_conversion_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  logits_QuantizeLinear_Input_0_conversion_layer, 49,
  NL_TYPE, 0x0, NULL,
  nl, node_convert,
  &logits_QuantizeLinear_Input_0_conversion_chain,
  NULL, &logits_QuantizeLinear_Input_0_conversion_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  logits_QuantizeLinear_Input_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_5_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &logits_QuantizeLinear_Input_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &logits_QuantizeLinear_Input_weights, &logits_QuantizeLinear_Input_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  logits_QuantizeLinear_Input_layer, 49,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &logits_QuantizeLinear_Input_chain,
  NULL, &logits_QuantizeLinear_Input_0_conversion_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  relu_5_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &mean_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_5_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &relu_5_weights, &relu_5_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  relu_5_layer, 46,
  DENSE_TYPE, 0x0, NULL,
  dense, forward_dense,
  &relu_5_chain,
  NULL, &logits_QuantizeLinear_Input_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  mean_Mul_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &mean_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &mean_Mul_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &mean_Mul_scale, &mean_Mul_bias),
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  mean_Mul_layer, 41,
  BN_TYPE, 0x0, NULL,
  bn, forward_bn,
  &mean_Mul_chain,
  NULL, &relu_5_layer, AI_STATIC, 
)


AI_STATIC_CONST ai_float mean_neutral_value_data[] = { 0.0f };
AI_ARRAY_OBJ_DECLARE(
    mean_neutral_value, AI_ARRAY_FORMAT_FLOAT,
    mean_neutral_value_data, mean_neutral_value_data, 1, AI_STATIC_CONST)
AI_TENSOR_CHAIN_OBJ_DECLARE(
  mean_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_4_0_0_mean_conversion_output0),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &mean_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  mean_layer, 41,
  REDUCE_TYPE, 0x0, NULL,
  reduce, forward_reduce,
  &mean_chain,
  NULL, &mean_Mul_layer, AI_STATIC, 
  .operation = ai_sum, 
  .neutral_value = &mean_neutral_value, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  relu_4_0_0_mean_conversion_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_4_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_4_0_0_mean_conversion_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  relu_4_0_0_mean_conversion_layer, 35,
  NL_TYPE, 0x0, NULL,
  nl, node_convert,
  &relu_4_0_0_mean_conversion_chain,
  NULL, &mean_layer, AI_STATIC, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  relu_4_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_3_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_4_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &relu_4_weights, &relu_4_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_4_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  relu_4_layer, 35,
  CONV2D_TYPE, 0x0, NULL,
  conv2d, forward_conv2d_integer_SSUA_ch,
  &relu_4_chain,
  NULL, &relu_4_0_0_mean_conversion_layer, AI_STATIC, 
  .groups = 1, 
  .filter_stride = AI_SHAPE_2D_INIT(1, 1), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 1, 0, 1, 0), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_SAME, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  relu_3_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_2_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_3_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &relu_3_weights, &relu_3_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &relu_3_scratch0, &relu_3_scratch1)
)

AI_LAYER_OBJ_DECLARE(
  relu_3_layer, 32,
  OPTIMIZED_CONV2D_TYPE, 0x0, NULL,
  conv2d_nl_pool, forward_conv2d_nl_pool_integer_SSUA_ch,
  &relu_3_chain,
  NULL, &relu_4_layer, AI_STATIC, 
  .groups = 1, 
  .filter_stride = AI_SHAPE_2D_INIT(1, 1), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 1, 0, 1, 0), 
  .pool_size = AI_SHAPE_2D_INIT(1, 2), 
  .pool_stride = AI_SHAPE_2D_INIT(1, 2), 
  .pool_pad = AI_SHAPE_INIT(4, 0, 0, 0, 0), 
  .pool_func = AI_HANDLE_PTR(pool_func_mp_array_integer_UINT8), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_SAME, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  relu_2_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_1_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_2_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &relu_2_weights, &relu_2_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_2_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  relu_2_layer, 26,
  CONV2D_TYPE, 0x0, NULL,
  conv2d, forward_conv2d_integer_SSUA_ch,
  &relu_2_chain,
  NULL, &relu_3_layer, AI_STATIC, 
  .groups = 1, 
  .filter_stride = AI_SHAPE_2D_INIT(1, 1), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 1, 0, 1, 0), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_SAME, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  relu_1_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_1_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &relu_1_weights, &relu_1_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 2, &relu_1_scratch0, &relu_1_scratch1)
)

AI_LAYER_OBJ_DECLARE(
  relu_1_layer, 23,
  OPTIMIZED_CONV2D_TYPE, 0x0, NULL,
  conv2d_nl_pool, forward_conv2d_nl_pool_integer_SSUA_ch,
  &relu_1_chain,
  NULL, &relu_2_layer, AI_STATIC, 
  .groups = 1, 
  .filter_stride = AI_SHAPE_2D_INIT(1, 1), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 2, 0, 2, 0), 
  .pool_size = AI_SHAPE_2D_INIT(1, 2), 
  .pool_stride = AI_SHAPE_2D_INIT(1, 2), 
  .pool_pad = AI_SHAPE_INIT(4, 0, 0, 0, 0), 
  .pool_func = AI_HANDLE_PTR(pool_func_mp_array_integer_UINT8), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_SAME, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  relu_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &imu_Transpose_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 3, &relu_weights, &relu_bias, NULL),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &relu_scratch0)
)

AI_LAYER_OBJ_DECLARE(
  relu_layer, 17,
  CONV2D_TYPE, 0x0, NULL,
  conv2d, forward_conv2d_integer_SSUA_ch,
  &relu_chain,
  NULL, &relu_1_layer, AI_STATIC, 
  .groups = 1, 
  .filter_stride = AI_SHAPE_2D_INIT(1, 1), 
  .dilation = AI_SHAPE_2D_INIT(1, 1), 
  .filter_pad = AI_SHAPE_INIT(4, 2, 0, 2, 0), 
  .in_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_SAME, 
  .out_ch_format = AI_LAYER_FORMAT_CHANNEL_LAST_VALID, 
)

AI_TENSOR_CHAIN_OBJ_DECLARE(
  imu_Transpose_chain, AI_STATIC_CONST, 4,
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &imu_output),
  AI_TENSOR_LIST_OBJ_INIT(AI_FLAG_NONE, 1, &imu_Transpose_output),
  AI_TENSOR_LIST_OBJ_EMPTY,
  AI_TENSOR_LIST_OBJ_EMPTY
)

AI_LAYER_OBJ_DECLARE(
  imu_Transpose_layer, 2,
  TRANSPOSE_TYPE, 0x0, NULL,
  transpose, forward_transpose,
  &imu_Transpose_chain,
  NULL, &relu_layer, AI_STATIC, 
  .out_mapping = AI_SHAPE_INIT(6, AI_SHAPE_IN_CHANNEL, AI_SHAPE_HEIGHT, AI_SHAPE_WIDTH, AI_SHAPE_CHANNEL, AI_SHAPE_DEPTH, AI_SHAPE_EXTENSION), 
)


#if (AI_TOOLS_API_VERSION < AI_TOOLS_API_VERSION_1_5)

AI_NETWORK_OBJ_DECLARE(
  AI_NET_OBJ_INSTANCE, AI_STATIC,
  AI_BUFFER_INIT(AI_FLAG_NONE,  AI_BUFFER_FORMAT_U8,
    AI_BUFFER_SHAPE_INIT(AI_SHAPE_BCWH, 4, 1, 86236, 1, 1),
    86236, NULL, NULL),
  AI_BUFFER_INIT(AI_FLAG_NONE,  AI_BUFFER_FORMAT_U8,
    AI_BUFFER_SHAPE_INIT(AI_SHAPE_BCWH, 4, 1, 66048, 1, 1),
    66048, NULL, NULL),
  AI_TENSOR_LIST_IO_OBJ_INIT(AI_FLAG_NONE, AI_HAR7CLASS_V6_INT8_IN_NUM, &imu_output),
  AI_TENSOR_LIST_IO_OBJ_INIT(AI_FLAG_NONE, AI_HAR7CLASS_V6_INT8_OUT_NUM, &logits_QuantizeLinear_Input_0_conversion_output),
  &imu_Transpose_layer, 0xcd9c6e2a, NULL)

#else

AI_NETWORK_OBJ_DECLARE(
  AI_NET_OBJ_INSTANCE, AI_STATIC,
  AI_BUFFER_ARRAY_OBJ_INIT_STATIC(
  	AI_FLAG_NONE, 1,
    AI_BUFFER_INIT(AI_FLAG_NONE,  AI_BUFFER_FORMAT_U8,
      AI_BUFFER_SHAPE_INIT(AI_SHAPE_BCWH, 4, 1, 86236, 1, 1),
      86236, NULL, NULL)
  ),
  AI_BUFFER_ARRAY_OBJ_INIT_STATIC(
  	AI_FLAG_NONE, 1,
    AI_BUFFER_INIT(AI_FLAG_NONE,  AI_BUFFER_FORMAT_U8,
      AI_BUFFER_SHAPE_INIT(AI_SHAPE_BCWH, 4, 1, 66048, 1, 1),
      66048, NULL, NULL)
  ),
  AI_TENSOR_LIST_IO_OBJ_INIT(AI_FLAG_NONE, AI_HAR7CLASS_V6_INT8_IN_NUM, &imu_output),
  AI_TENSOR_LIST_IO_OBJ_INIT(AI_FLAG_NONE, AI_HAR7CLASS_V6_INT8_OUT_NUM, &logits_QuantizeLinear_Input_0_conversion_output),
  &imu_Transpose_layer, 0xcd9c6e2a, NULL)

#endif	/*(AI_TOOLS_API_VERSION < AI_TOOLS_API_VERSION_1_5)*/



/******************************************************************************/
AI_DECLARE_STATIC
ai_bool har7class_v6_int8_configure_activations(
  ai_network* net_ctx, const ai_network_params* params)
{
  AI_ASSERT(net_ctx)

  if (ai_platform_get_activations_map(g_har7class_v6_int8_activations_map, 1, params)) {
    /* Updating activations (byte) offsets */
    
    imu_output_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 26944);
    imu_output_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 26944);
    imu_Transpose_output_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 30016);
    imu_Transpose_output_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 30016);
    relu_scratch0_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 26944);
    relu_scratch0_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 26944);
    relu_output_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 33088);
    relu_output_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 33088);
    relu_1_scratch0_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 26880);
    relu_1_scratch0_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 26880);
    relu_1_scratch1_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 49472);
    relu_1_scratch1_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 49472);
    relu_1_output_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 49536);
    relu_1_output_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 49536);
    relu_2_scratch0_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 26880);
    relu_2_scratch0_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 26880);
    relu_2_output_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 41152);
    relu_2_output_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 41152);
    relu_3_scratch0_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 26880);
    relu_3_scratch0_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 26880);
    relu_3_scratch1_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 33664);
    relu_3_scratch1_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 33664);
    relu_3_output_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 40960);
    relu_3_output_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 40960);
    relu_4_scratch0_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 26880);
    relu_4_scratch0_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 26880);
    relu_4_output_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 49152);
    relu_4_output_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 49152);
    relu_4_0_0_mean_conversion_output_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 0);
    relu_4_0_0_mean_conversion_output_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 0);
    mean_output_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 65536);
    mean_output_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 65536);
    mean_Mul_output_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 0);
    mean_Mul_output_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 0);
    relu_5_output_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 512);
    relu_5_output_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 512);
    logits_QuantizeLinear_Input_output_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 0);
    logits_QuantizeLinear_Input_output_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 0);
    logits_QuantizeLinear_Input_0_conversion_output_array.data = AI_PTR(g_har7class_v6_int8_activations_map[0] + 28);
    logits_QuantizeLinear_Input_0_conversion_output_array.data_start = AI_PTR(g_har7class_v6_int8_activations_map[0] + 28);
    return true;
  }
  AI_ERROR_TRAP(net_ctx, INIT_FAILED, NETWORK_ACTIVATIONS);
  return false;
}




/******************************************************************************/
AI_DECLARE_STATIC
ai_bool har7class_v6_int8_configure_weights(
  ai_network* net_ctx, const ai_network_params* params)
{
  AI_ASSERT(net_ctx)

  if (ai_platform_get_weights_map(g_har7class_v6_int8_weights_map, 1, params)) {
    /* Updating weights (byte) offsets */
    
    relu_weights_array.format |= AI_FMT_FLAG_CONST;
    relu_weights_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 0);
    relu_weights_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 0);
    relu_bias_array.format |= AI_FMT_FLAG_CONST;
    relu_bias_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 960);
    relu_bias_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 960);
    relu_1_weights_array.format |= AI_FMT_FLAG_CONST;
    relu_1_weights_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 1088);
    relu_1_weights_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 1088);
    relu_1_bias_array.format |= AI_FMT_FLAG_CONST;
    relu_1_bias_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 6208);
    relu_1_bias_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 6208);
    relu_2_weights_array.format |= AI_FMT_FLAG_CONST;
    relu_2_weights_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 6336);
    relu_2_weights_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 6336);
    relu_2_bias_array.format |= AI_FMT_FLAG_CONST;
    relu_2_bias_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 12480);
    relu_2_bias_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 12480);
    relu_3_weights_array.format |= AI_FMT_FLAG_CONST;
    relu_3_weights_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 12736);
    relu_3_weights_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 12736);
    relu_3_bias_array.format |= AI_FMT_FLAG_CONST;
    relu_3_bias_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 25024);
    relu_3_bias_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 25024);
    relu_4_weights_array.format |= AI_FMT_FLAG_CONST;
    relu_4_weights_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 25280);
    relu_4_weights_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 25280);
    relu_4_bias_array.format |= AI_FMT_FLAG_CONST;
    relu_4_bias_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 49856);
    relu_4_bias_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 49856);
    mean_Mul_scale_array.format |= AI_FMT_FLAG_CONST;
    mean_Mul_scale_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 50368);
    mean_Mul_scale_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 50368);
    mean_Mul_bias_array.format |= AI_FMT_FLAG_CONST;
    mean_Mul_bias_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 50880);
    mean_Mul_bias_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 50880);
    relu_5_weights_array.format |= AI_FMT_FLAG_CONST;
    relu_5_weights_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 51392);
    relu_5_weights_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 51392);
    relu_5_bias_array.format |= AI_FMT_FLAG_CONST;
    relu_5_bias_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 84160);
    relu_5_bias_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 84160);
    logits_QuantizeLinear_Input_weights_array.format |= AI_FMT_FLAG_CONST;
    logits_QuantizeLinear_Input_weights_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 84416);
    logits_QuantizeLinear_Input_weights_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 84416);
    logits_QuantizeLinear_Input_bias_array.format |= AI_FMT_FLAG_CONST;
    logits_QuantizeLinear_Input_bias_array.data = AI_PTR(g_har7class_v6_int8_weights_map[0] + 86208);
    logits_QuantizeLinear_Input_bias_array.data_start = AI_PTR(g_har7class_v6_int8_weights_map[0] + 86208);
    return true;
  }
  AI_ERROR_TRAP(net_ctx, INIT_FAILED, NETWORK_WEIGHTS);
  return false;
}


/**  PUBLIC APIs SECTION  *****************************************************/



AI_DEPRECATED
AI_API_ENTRY
ai_bool ai_har7class_v6_int8_get_info(
  ai_handle network, ai_network_report* report)
{
  ai_network* net_ctx = AI_NETWORK_ACQUIRE_CTX(network);

  if (report && net_ctx)
  {
    ai_network_report r = {
      .model_name        = AI_HAR7CLASS_V6_INT8_MODEL_NAME,
      .model_signature   = AI_HAR7CLASS_V6_INT8_MODEL_SIGNATURE,
      .model_datetime    = AI_TOOLS_DATE_TIME,
      
      .compile_datetime  = AI_TOOLS_COMPILE_TIME,
      
      .runtime_revision  = ai_platform_runtime_get_revision(),
      .runtime_version   = ai_platform_runtime_get_version(),

      .tool_revision     = AI_TOOLS_REVISION_ID,
      .tool_version      = {AI_TOOLS_VERSION_MAJOR, AI_TOOLS_VERSION_MINOR,
                            AI_TOOLS_VERSION_MICRO, 0x0},
      .tool_api_version  = AI_STRUCT_INIT,

      .api_version            = ai_platform_api_get_version(),
      .interface_api_version  = ai_platform_interface_api_get_version(),
      
      .n_macc            = 11070037,
      .n_inputs          = 0,
      .inputs            = NULL,
      .n_outputs         = 0,
      .outputs           = NULL,
      .params            = AI_STRUCT_INIT,
      .activations       = AI_STRUCT_INIT,
      .n_nodes           = 0,
      .signature         = 0xcd9c6e2a,
    };

    if (!ai_platform_api_get_network_report(network, &r)) return false;

    *report = r;
    return true;
  }
  return false;
}



AI_API_ENTRY
ai_bool ai_har7class_v6_int8_get_report(
  ai_handle network, ai_network_report* report)
{
  ai_network* net_ctx = AI_NETWORK_ACQUIRE_CTX(network);

  if (report && net_ctx)
  {
    ai_network_report r = {
      .model_name        = AI_HAR7CLASS_V6_INT8_MODEL_NAME,
      .model_signature   = AI_HAR7CLASS_V6_INT8_MODEL_SIGNATURE,
      .model_datetime    = AI_TOOLS_DATE_TIME,
      
      .compile_datetime  = AI_TOOLS_COMPILE_TIME,
      
      .runtime_revision  = ai_platform_runtime_get_revision(),
      .runtime_version   = ai_platform_runtime_get_version(),

      .tool_revision     = AI_TOOLS_REVISION_ID,
      .tool_version      = {AI_TOOLS_VERSION_MAJOR, AI_TOOLS_VERSION_MINOR,
                            AI_TOOLS_VERSION_MICRO, 0x0},
      .tool_api_version  = AI_STRUCT_INIT,

      .api_version            = ai_platform_api_get_version(),
      .interface_api_version  = ai_platform_interface_api_get_version(),
      
      .n_macc            = 11070037,
      .n_inputs          = 0,
      .inputs            = NULL,
      .n_outputs         = 0,
      .outputs           = NULL,
      .map_signature     = AI_MAGIC_SIGNATURE,
      .map_weights       = AI_STRUCT_INIT,
      .map_activations   = AI_STRUCT_INIT,
      .n_nodes           = 0,
      .signature         = 0xcd9c6e2a,
    };

    if (!ai_platform_api_get_network_report(network, &r)) return false;

    *report = r;
    return true;
  }
  return false;
}


AI_API_ENTRY
ai_error ai_har7class_v6_int8_get_error(ai_handle network)
{
  return ai_platform_network_get_error(network);
}


AI_API_ENTRY
ai_error ai_har7class_v6_int8_create(
  ai_handle* network, const ai_buffer* network_config)
{
  return ai_platform_network_create(
    network, network_config, 
    AI_CONTEXT_OBJ(&AI_NET_OBJ_INSTANCE),
    AI_TOOLS_API_VERSION_MAJOR, AI_TOOLS_API_VERSION_MINOR, AI_TOOLS_API_VERSION_MICRO);
}


AI_API_ENTRY
ai_error ai_har7class_v6_int8_create_and_init(
  ai_handle* network, const ai_handle activations[], const ai_handle weights[])
{
  ai_error err;
  ai_network_params params;

  err = ai_har7class_v6_int8_create(network, AI_HAR7CLASS_V6_INT8_DATA_CONFIG);
  if (err.type != AI_ERROR_NONE) {
    return err;
  }
  
  if (ai_har7class_v6_int8_data_params_get(&params) != true) {
    err = ai_har7class_v6_int8_get_error(*network);
    return err;
  }
#if defined(AI_HAR7CLASS_V6_INT8_DATA_ACTIVATIONS_COUNT)
  /* set the addresses of the activations buffers */
  for (ai_u16 idx=0; activations && idx<params.map_activations.size; idx++) {
    AI_BUFFER_ARRAY_ITEM_SET_ADDRESS(&params.map_activations, idx, activations[idx]);
  }
#endif
#if defined(AI_HAR7CLASS_V6_INT8_DATA_WEIGHTS_COUNT)
  /* set the addresses of the weight buffers */
  for (ai_u16 idx=0; weights && idx<params.map_weights.size; idx++) {
    AI_BUFFER_ARRAY_ITEM_SET_ADDRESS(&params.map_weights, idx, weights[idx]);
  }
#endif
  if (ai_har7class_v6_int8_init(*network, &params) != true) {
    err = ai_har7class_v6_int8_get_error(*network);
  }
  return err;
}


AI_API_ENTRY
ai_buffer* ai_har7class_v6_int8_inputs_get(ai_handle network, ai_u16 *n_buffer)
{
  if (network == AI_HANDLE_NULL) {
    network = (ai_handle)&AI_NET_OBJ_INSTANCE;
    AI_NETWORK_OBJ(network)->magic = AI_MAGIC_CONTEXT_TOKEN;
  }
  return ai_platform_inputs_get(network, n_buffer);
}


AI_API_ENTRY
ai_buffer* ai_har7class_v6_int8_outputs_get(ai_handle network, ai_u16 *n_buffer)
{
  if (network == AI_HANDLE_NULL) {
    network = (ai_handle)&AI_NET_OBJ_INSTANCE;
    AI_NETWORK_OBJ(network)->magic = AI_MAGIC_CONTEXT_TOKEN;
  }
  return ai_platform_outputs_get(network, n_buffer);
}


AI_API_ENTRY
ai_handle ai_har7class_v6_int8_destroy(ai_handle network)
{
  return ai_platform_network_destroy(network);
}


AI_API_ENTRY
ai_bool ai_har7class_v6_int8_init(
  ai_handle network, const ai_network_params* params)
{
  ai_network* net_ctx = AI_NETWORK_OBJ(ai_platform_network_init(network, params));
  ai_bool ok = true;

  if (!net_ctx) return false;
  ok &= har7class_v6_int8_configure_weights(net_ctx, params);
  ok &= har7class_v6_int8_configure_activations(net_ctx, params);

  ok &= ai_platform_network_post_init(network);

  return ok;
}


AI_API_ENTRY
ai_i32 ai_har7class_v6_int8_run(
  ai_handle network, const ai_buffer* input, ai_buffer* output)
{
  return ai_platform_network_process(network, input, output);
}


AI_API_ENTRY
ai_i32 ai_har7class_v6_int8_forward(ai_handle network, const ai_buffer* input)
{
  return ai_platform_network_process(network, input, NULL);
}



#undef AI_HAR7CLASS_V6_INT8_MODEL_SIGNATURE
#undef AI_NET_OBJ_INSTANCE
#undef AI_TOOLS_DATE_TIME
#undef AI_TOOLS_COMPILE_TIME

