; ModuleID = 'stencil.bc'
source_filename = "ld-temp.o"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct._IO_FILE = type { i32, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, %struct._IO_marker*, %struct._IO_FILE*, i32, i32, i64, i16, i8, [1 x i8], i8*, i64, i8*, i8*, i8*, i8*, i64, i32, [20 x i8] }
%struct._IO_marker = type { %struct._IO_marker*, %struct._IO_FILE*, i32 }
%struct.stat = type { i64, i64, i64, i32, i32, i32, i32, i64, i64, i64, i64, %struct.timespec, %struct.timespec, %struct.timespec, [3 x i64] }
%struct.timespec = type { i64, i64 }
%struct.__va_list_tag = type { i32, i32, i8*, i8* }

@INPUT_SIZE = internal dso_local global i32 131080, align 4
@.str.1 = private unnamed_addr constant [34 x i8] c"fd>1 && \22Invalid file descriptor\22\00", align 1
@.str.2 = private unnamed_addr constant [23 x i8] c"../../common/support.c\00", align 1
@__PRETTY_FUNCTION__.readfile = private unnamed_addr constant [20 x i8] c"char *readfile(int)\00", align 1
@.str.4 = private unnamed_addr constant [51 x i8] c"0==fstat(fd, &s) && \22Couldn't determine file size\22\00", align 1
@.str.6 = private unnamed_addr constant [25 x i8] c"len>0 && \22File is empty\22\00", align 1
@.str.8 = private unnamed_addr constant [29 x i8] c"status>=0 && \22read() failed\22\00", align 1
@.str.10 = private unnamed_addr constant [33 x i8] c"n>=0 && \22Invalid section number\22\00", align 1
@__PRETTY_FUNCTION__.find_section_start = private unnamed_addr constant [38 x i8] c"char *find_section_start(char *, int)\00", align 1
@.str.12 = private unnamed_addr constant [34 x i8] c"s!=NULL && \22Invalid input string\22\00", align 1
@__PRETTY_FUNCTION__.parse_string = private unnamed_addr constant [38 x i8] c"int parse_string(char *, char *, int)\00", align 1
@__PRETTY_FUNCTION__.parse_uint8_t_array = private unnamed_addr constant [48 x i8] c"int parse_uint8_t_array(char *, uint8_t *, int)\00", align 1
@.str.13 = private unnamed_addr constant [2 x i8] c"\0A\00", align 1
@stderr = external local_unnamed_addr global %struct._IO_FILE*, align 8
@.str.14 = private unnamed_addr constant [35 x i8] c"Invalid input: line %d of section\0A\00", align 1
@__PRETTY_FUNCTION__.parse_uint16_t_array = private unnamed_addr constant [50 x i8] c"int parse_uint16_t_array(char *, uint16_t *, int)\00", align 1
@__PRETTY_FUNCTION__.parse_uint32_t_array = private unnamed_addr constant [50 x i8] c"int parse_uint32_t_array(char *, uint32_t *, int)\00", align 1
@__PRETTY_FUNCTION__.parse_uint64_t_array = private unnamed_addr constant [50 x i8] c"int parse_uint64_t_array(char *, uint64_t *, int)\00", align 1
@__PRETTY_FUNCTION__.parse_int8_t_array = private unnamed_addr constant [46 x i8] c"int parse_int8_t_array(char *, int8_t *, int)\00", align 1
@__PRETTY_FUNCTION__.parse_int16_t_array = private unnamed_addr constant [48 x i8] c"int parse_int16_t_array(char *, int16_t *, int)\00", align 1
@__PRETTY_FUNCTION__.parse_int32_t_array = private unnamed_addr constant [48 x i8] c"int parse_int32_t_array(char *, int32_t *, int)\00", align 1
@__PRETTY_FUNCTION__.parse_int64_t_array = private unnamed_addr constant [48 x i8] c"int parse_int64_t_array(char *, int64_t *, int)\00", align 1
@__PRETTY_FUNCTION__.parse_float_array = private unnamed_addr constant [44 x i8] c"int parse_float_array(char *, float *, int)\00", align 1
@__PRETTY_FUNCTION__.parse_double_array = private unnamed_addr constant [46 x i8] c"int parse_double_array(char *, double *, int)\00", align 1
@__PRETTY_FUNCTION__.write_string = private unnamed_addr constant [35 x i8] c"int write_string(int, char *, int)\00", align 1
@.str.16 = private unnamed_addr constant [28 x i8] c"status>=0 && \22Write failed\22\00", align 1
@__PRETTY_FUNCTION__.write_uint8_t_array = private unnamed_addr constant [45 x i8] c"int write_uint8_t_array(int, uint8_t *, int)\00", align 1
@.str.17 = private unnamed_addr constant [4 x i8] c"%u\0A\00", align 1
@.str.24 = private unnamed_addr constant [90 x i8] c"buffered<SUFFICIENT_SPRINTF_SPACE && \22Overran fd_printf buffer---output possibly corrupt\22\00", align 1
@__PRETTY_FUNCTION__.fd_printf = private unnamed_addr constant [38 x i8] c"int fd_printf(int, const char *, ...)\00", align 1
@.str.26 = private unnamed_addr constant [50 x i8] c"written==buffered && \22Wrote more data than given\22\00", align 1
@__PRETTY_FUNCTION__.write_uint16_t_array = private unnamed_addr constant [47 x i8] c"int write_uint16_t_array(int, uint16_t *, int)\00", align 1
@__PRETTY_FUNCTION__.write_uint32_t_array = private unnamed_addr constant [47 x i8] c"int write_uint32_t_array(int, uint32_t *, int)\00", align 1
@__PRETTY_FUNCTION__.write_uint64_t_array = private unnamed_addr constant [47 x i8] c"int write_uint64_t_array(int, uint64_t *, int)\00", align 1
@.str.18 = private unnamed_addr constant [5 x i8] c"%lu\0A\00", align 1
@__PRETTY_FUNCTION__.write_int8_t_array = private unnamed_addr constant [43 x i8] c"int write_int8_t_array(int, int8_t *, int)\00", align 1
@.str.19 = private unnamed_addr constant [4 x i8] c"%d\0A\00", align 1
@__PRETTY_FUNCTION__.write_int16_t_array = private unnamed_addr constant [45 x i8] c"int write_int16_t_array(int, int16_t *, int)\00", align 1
@__PRETTY_FUNCTION__.write_int32_t_array = private unnamed_addr constant [45 x i8] c"int write_int32_t_array(int, int32_t *, int)\00", align 1
@__PRETTY_FUNCTION__.write_int64_t_array = private unnamed_addr constant [45 x i8] c"int write_int64_t_array(int, int64_t *, int)\00", align 1
@.str.20 = private unnamed_addr constant [5 x i8] c"%ld\0A\00", align 1
@__PRETTY_FUNCTION__.write_float_array = private unnamed_addr constant [41 x i8] c"int write_float_array(int, float *, int)\00", align 1
@.str.21 = private unnamed_addr constant [7 x i8] c"%.16f\0A\00", align 1
@__PRETTY_FUNCTION__.write_double_array = private unnamed_addr constant [43 x i8] c"int write_double_array(int, double *, int)\00", align 1
@__PRETTY_FUNCTION__.write_section_header = private unnamed_addr constant [30 x i8] c"int write_section_header(int)\00", align 1
@.str.22 = private unnamed_addr constant [6 x i8] c"%%%%\0A\00", align 1
@.str.1.11 = private unnamed_addr constant [57 x i8] c"argc<4 && \22Usage: ./benchmark <input_file> <check_file>\22\00", align 1
@.str.2.12 = private unnamed_addr constant [23 x i8] c"../../common/harness.c\00", align 1
@__PRETTY_FUNCTION__.main = private unnamed_addr constant [23 x i8] c"int main(int, char **)\00", align 1
@.str.3 = private unnamed_addr constant [11 x i8] c"input.data\00", align 1
@.str.5 = private unnamed_addr constant [30 x i8] c"data!=NULL && \22Out of memory\22\00", align 1
@.str.7 = private unnamed_addr constant [43 x i8] c"in_fd>0 && \22Couldn't open input data file\22\00", align 1
@str = private unnamed_addr constant [9 x i8] c"Success.\00"

; Function Attrs: noinline norecurse nounwind uwtable
define internal dso_local void @kernel(i32* nocapture readonly %arg, i32* nocapture readonly %arg1, i32* nocapture %arg2) #0 {
bb:
  br label %bb3

bb3:                                              ; preds = %bb3, %bb
  %tmp = phi i64 [ 0, %bb ], [ %tmp132, %bb3 ]
  %tmp4 = shl i64 %tmp, 4
  %tmp5 = getelementptr inbounds i32, i32* %arg1, i64 %tmp4
  %tmp6 = load i32, i32* %tmp5, align 4, !tbaa !2
  %tmp7 = getelementptr inbounds i32, i32* %arg2, i64 %tmp4
  store i32 %tmp6, i32* %tmp7, align 4, !tbaa !2
  %tmp8 = add nuw nsw i64 %tmp4, 15872
  %tmp9 = getelementptr inbounds i32, i32* %arg1, i64 %tmp8
  %tmp10 = load i32, i32* %tmp9, align 4, !tbaa !2
  %tmp11 = getelementptr inbounds i32, i32* %arg2, i64 %tmp8
  store i32 %tmp10, i32* %tmp11, align 4, !tbaa !2
  %tmp12 = or i64 %tmp4, 1
  %tmp13 = getelementptr inbounds i32, i32* %arg1, i64 %tmp12
  %tmp14 = load i32, i32* %tmp13, align 4, !tbaa !2
  %tmp15 = getelementptr inbounds i32, i32* %arg2, i64 %tmp12
  store i32 %tmp14, i32* %tmp15, align 4, !tbaa !2
  %tmp16 = add nuw nsw i64 %tmp12, 15872
  %tmp17 = getelementptr inbounds i32, i32* %arg1, i64 %tmp16
  %tmp18 = load i32, i32* %tmp17, align 4, !tbaa !2
  %tmp19 = getelementptr inbounds i32, i32* %arg2, i64 %tmp16
  store i32 %tmp18, i32* %tmp19, align 4, !tbaa !2
  %tmp20 = or i64 %tmp4, 2
  %tmp21 = getelementptr inbounds i32, i32* %arg1, i64 %tmp20
  %tmp22 = load i32, i32* %tmp21, align 4, !tbaa !2
  %tmp23 = getelementptr inbounds i32, i32* %arg2, i64 %tmp20
  store i32 %tmp22, i32* %tmp23, align 4, !tbaa !2
  %tmp24 = add nuw nsw i64 %tmp20, 15872
  %tmp25 = getelementptr inbounds i32, i32* %arg1, i64 %tmp24
  %tmp26 = load i32, i32* %tmp25, align 4, !tbaa !2
  %tmp27 = getelementptr inbounds i32, i32* %arg2, i64 %tmp24
  store i32 %tmp26, i32* %tmp27, align 4, !tbaa !2
  %tmp28 = or i64 %tmp4, 3
  %tmp29 = getelementptr inbounds i32, i32* %arg1, i64 %tmp28
  %tmp30 = load i32, i32* %tmp29, align 4, !tbaa !2
  %tmp31 = getelementptr inbounds i32, i32* %arg2, i64 %tmp28
  store i32 %tmp30, i32* %tmp31, align 4, !tbaa !2
  %tmp32 = add nuw nsw i64 %tmp28, 15872
  %tmp33 = getelementptr inbounds i32, i32* %arg1, i64 %tmp32
  %tmp34 = load i32, i32* %tmp33, align 4, !tbaa !2
  %tmp35 = getelementptr inbounds i32, i32* %arg2, i64 %tmp32
  store i32 %tmp34, i32* %tmp35, align 4, !tbaa !2
  %tmp36 = or i64 %tmp4, 4
  %tmp37 = getelementptr inbounds i32, i32* %arg1, i64 %tmp36
  %tmp38 = load i32, i32* %tmp37, align 4, !tbaa !2
  %tmp39 = getelementptr inbounds i32, i32* %arg2, i64 %tmp36
  store i32 %tmp38, i32* %tmp39, align 4, !tbaa !2
  %tmp40 = add nuw nsw i64 %tmp36, 15872
  %tmp41 = getelementptr inbounds i32, i32* %arg1, i64 %tmp40
  %tmp42 = load i32, i32* %tmp41, align 4, !tbaa !2
  %tmp43 = getelementptr inbounds i32, i32* %arg2, i64 %tmp40
  store i32 %tmp42, i32* %tmp43, align 4, !tbaa !2
  %tmp44 = or i64 %tmp4, 5
  %tmp45 = getelementptr inbounds i32, i32* %arg1, i64 %tmp44
  %tmp46 = load i32, i32* %tmp45, align 4, !tbaa !2
  %tmp47 = getelementptr inbounds i32, i32* %arg2, i64 %tmp44
  store i32 %tmp46, i32* %tmp47, align 4, !tbaa !2
  %tmp48 = add nuw nsw i64 %tmp44, 15872
  %tmp49 = getelementptr inbounds i32, i32* %arg1, i64 %tmp48
  %tmp50 = load i32, i32* %tmp49, align 4, !tbaa !2
  %tmp51 = getelementptr inbounds i32, i32* %arg2, i64 %tmp48
  store i32 %tmp50, i32* %tmp51, align 4, !tbaa !2
  %tmp52 = or i64 %tmp4, 6
  %tmp53 = getelementptr inbounds i32, i32* %arg1, i64 %tmp52
  %tmp54 = load i32, i32* %tmp53, align 4, !tbaa !2
  %tmp55 = getelementptr inbounds i32, i32* %arg2, i64 %tmp52
  store i32 %tmp54, i32* %tmp55, align 4, !tbaa !2
  %tmp56 = add nuw nsw i64 %tmp52, 15872
  %tmp57 = getelementptr inbounds i32, i32* %arg1, i64 %tmp56
  %tmp58 = load i32, i32* %tmp57, align 4, !tbaa !2
  %tmp59 = getelementptr inbounds i32, i32* %arg2, i64 %tmp56
  store i32 %tmp58, i32* %tmp59, align 4, !tbaa !2
  %tmp60 = or i64 %tmp4, 7
  %tmp61 = getelementptr inbounds i32, i32* %arg1, i64 %tmp60
  %tmp62 = load i32, i32* %tmp61, align 4, !tbaa !2
  %tmp63 = getelementptr inbounds i32, i32* %arg2, i64 %tmp60
  store i32 %tmp62, i32* %tmp63, align 4, !tbaa !2
  %tmp64 = add nuw nsw i64 %tmp60, 15872
  %tmp65 = getelementptr inbounds i32, i32* %arg1, i64 %tmp64
  %tmp66 = load i32, i32* %tmp65, align 4, !tbaa !2
  %tmp67 = getelementptr inbounds i32, i32* %arg2, i64 %tmp64
  store i32 %tmp66, i32* %tmp67, align 4, !tbaa !2
  %tmp68 = or i64 %tmp4, 8
  %tmp69 = getelementptr inbounds i32, i32* %arg1, i64 %tmp68
  %tmp70 = load i32, i32* %tmp69, align 4, !tbaa !2
  %tmp71 = getelementptr inbounds i32, i32* %arg2, i64 %tmp68
  store i32 %tmp70, i32* %tmp71, align 4, !tbaa !2
  %tmp72 = add nuw nsw i64 %tmp68, 15872
  %tmp73 = getelementptr inbounds i32, i32* %arg1, i64 %tmp72
  %tmp74 = load i32, i32* %tmp73, align 4, !tbaa !2
  %tmp75 = getelementptr inbounds i32, i32* %arg2, i64 %tmp72
  store i32 %tmp74, i32* %tmp75, align 4, !tbaa !2
  %tmp76 = or i64 %tmp4, 9
  %tmp77 = getelementptr inbounds i32, i32* %arg1, i64 %tmp76
  %tmp78 = load i32, i32* %tmp77, align 4, !tbaa !2
  %tmp79 = getelementptr inbounds i32, i32* %arg2, i64 %tmp76
  store i32 %tmp78, i32* %tmp79, align 4, !tbaa !2
  %tmp80 = add nuw nsw i64 %tmp76, 15872
  %tmp81 = getelementptr inbounds i32, i32* %arg1, i64 %tmp80
  %tmp82 = load i32, i32* %tmp81, align 4, !tbaa !2
  %tmp83 = getelementptr inbounds i32, i32* %arg2, i64 %tmp80
  store i32 %tmp82, i32* %tmp83, align 4, !tbaa !2
  %tmp84 = or i64 %tmp4, 10
  %tmp85 = getelementptr inbounds i32, i32* %arg1, i64 %tmp84
  %tmp86 = load i32, i32* %tmp85, align 4, !tbaa !2
  %tmp87 = getelementptr inbounds i32, i32* %arg2, i64 %tmp84
  store i32 %tmp86, i32* %tmp87, align 4, !tbaa !2
  %tmp88 = add nuw nsw i64 %tmp84, 15872
  %tmp89 = getelementptr inbounds i32, i32* %arg1, i64 %tmp88
  %tmp90 = load i32, i32* %tmp89, align 4, !tbaa !2
  %tmp91 = getelementptr inbounds i32, i32* %arg2, i64 %tmp88
  store i32 %tmp90, i32* %tmp91, align 4, !tbaa !2
  %tmp92 = or i64 %tmp4, 11
  %tmp93 = getelementptr inbounds i32, i32* %arg1, i64 %tmp92
  %tmp94 = load i32, i32* %tmp93, align 4, !tbaa !2
  %tmp95 = getelementptr inbounds i32, i32* %arg2, i64 %tmp92
  store i32 %tmp94, i32* %tmp95, align 4, !tbaa !2
  %tmp96 = add nuw nsw i64 %tmp92, 15872
  %tmp97 = getelementptr inbounds i32, i32* %arg1, i64 %tmp96
  %tmp98 = load i32, i32* %tmp97, align 4, !tbaa !2
  %tmp99 = getelementptr inbounds i32, i32* %arg2, i64 %tmp96
  store i32 %tmp98, i32* %tmp99, align 4, !tbaa !2
  %tmp100 = or i64 %tmp4, 12
  %tmp101 = getelementptr inbounds i32, i32* %arg1, i64 %tmp100
  %tmp102 = load i32, i32* %tmp101, align 4, !tbaa !2
  %tmp103 = getelementptr inbounds i32, i32* %arg2, i64 %tmp100
  store i32 %tmp102, i32* %tmp103, align 4, !tbaa !2
  %tmp104 = add nuw nsw i64 %tmp100, 15872
  %tmp105 = getelementptr inbounds i32, i32* %arg1, i64 %tmp104
  %tmp106 = load i32, i32* %tmp105, align 4, !tbaa !2
  %tmp107 = getelementptr inbounds i32, i32* %arg2, i64 %tmp104
  store i32 %tmp106, i32* %tmp107, align 4, !tbaa !2
  %tmp108 = or i64 %tmp4, 13
  %tmp109 = getelementptr inbounds i32, i32* %arg1, i64 %tmp108
  %tmp110 = load i32, i32* %tmp109, align 4, !tbaa !2
  %tmp111 = getelementptr inbounds i32, i32* %arg2, i64 %tmp108
  store i32 %tmp110, i32* %tmp111, align 4, !tbaa !2
  %tmp112 = add nuw nsw i64 %tmp108, 15872
  %tmp113 = getelementptr inbounds i32, i32* %arg1, i64 %tmp112
  %tmp114 = load i32, i32* %tmp113, align 4, !tbaa !2
  %tmp115 = getelementptr inbounds i32, i32* %arg2, i64 %tmp112
  store i32 %tmp114, i32* %tmp115, align 4, !tbaa !2
  %tmp116 = or i64 %tmp4, 14
  %tmp117 = getelementptr inbounds i32, i32* %arg1, i64 %tmp116
  %tmp118 = load i32, i32* %tmp117, align 4, !tbaa !2
  %tmp119 = getelementptr inbounds i32, i32* %arg2, i64 %tmp116
  store i32 %tmp118, i32* %tmp119, align 4, !tbaa !2
  %tmp120 = add nuw nsw i64 %tmp116, 15872
  %tmp121 = getelementptr inbounds i32, i32* %arg1, i64 %tmp120
  %tmp122 = load i32, i32* %tmp121, align 4, !tbaa !2
  %tmp123 = getelementptr inbounds i32, i32* %arg2, i64 %tmp120
  store i32 %tmp122, i32* %tmp123, align 4, !tbaa !2
  %tmp124 = or i64 %tmp4, 15
  %tmp125 = getelementptr inbounds i32, i32* %arg1, i64 %tmp124
  %tmp126 = load i32, i32* %tmp125, align 4, !tbaa !2
  %tmp127 = getelementptr inbounds i32, i32* %arg2, i64 %tmp124
  store i32 %tmp126, i32* %tmp127, align 4, !tbaa !2
  %tmp128 = add nuw nsw i64 %tmp124, 15872
  %tmp129 = getelementptr inbounds i32, i32* %arg1, i64 %tmp128
  %tmp130 = load i32, i32* %tmp129, align 4, !tbaa !2
  %tmp131 = getelementptr inbounds i32, i32* %arg2, i64 %tmp128
  store i32 %tmp130, i32* %tmp131, align 4, !tbaa !2
  %tmp132 = add nuw nsw i64 %tmp, 1
  %tmp133 = icmp eq i64 %tmp132, 32
  br i1 %tmp133, label %bb134, label %bb3

bb134:                                            ; preds = %bb3
  br label %bb135

bb135:                                            ; preds = %bb135, %bb134
  %tmp136 = phi i64 [ %tmp265, %bb135 ], [ 1, %bb134 ]
  %tmp137 = shl i64 %tmp136, 9
  %tmp138 = or i64 %tmp137, 496
  %tmp139 = getelementptr inbounds i32, i32* %arg1, i64 %tmp137
  %tmp140 = load i32, i32* %tmp139, align 4, !tbaa !2
  %tmp141 = getelementptr inbounds i32, i32* %arg2, i64 %tmp137
  store i32 %tmp140, i32* %tmp141, align 4, !tbaa !2
  %tmp142 = getelementptr inbounds i32, i32* %arg1, i64 %tmp138
  %tmp143 = load i32, i32* %tmp142, align 4, !tbaa !2
  %tmp144 = getelementptr inbounds i32, i32* %arg2, i64 %tmp138
  store i32 %tmp143, i32* %tmp144, align 4, !tbaa !2
  %tmp145 = or i64 %tmp137, 1
  %tmp146 = getelementptr inbounds i32, i32* %arg1, i64 %tmp145
  %tmp147 = load i32, i32* %tmp146, align 4, !tbaa !2
  %tmp148 = getelementptr inbounds i32, i32* %arg2, i64 %tmp145
  store i32 %tmp147, i32* %tmp148, align 4, !tbaa !2
  %tmp149 = or i64 %tmp137, 497
  %tmp150 = getelementptr inbounds i32, i32* %arg1, i64 %tmp149
  %tmp151 = load i32, i32* %tmp150, align 4, !tbaa !2
  %tmp152 = getelementptr inbounds i32, i32* %arg2, i64 %tmp149
  store i32 %tmp151, i32* %tmp152, align 4, !tbaa !2
  %tmp153 = or i64 %tmp137, 2
  %tmp154 = getelementptr inbounds i32, i32* %arg1, i64 %tmp153
  %tmp155 = load i32, i32* %tmp154, align 4, !tbaa !2
  %tmp156 = getelementptr inbounds i32, i32* %arg2, i64 %tmp153
  store i32 %tmp155, i32* %tmp156, align 4, !tbaa !2
  %tmp157 = or i64 %tmp137, 498
  %tmp158 = getelementptr inbounds i32, i32* %arg1, i64 %tmp157
  %tmp159 = load i32, i32* %tmp158, align 4, !tbaa !2
  %tmp160 = getelementptr inbounds i32, i32* %arg2, i64 %tmp157
  store i32 %tmp159, i32* %tmp160, align 4, !tbaa !2
  %tmp161 = or i64 %tmp137, 3
  %tmp162 = getelementptr inbounds i32, i32* %arg1, i64 %tmp161
  %tmp163 = load i32, i32* %tmp162, align 4, !tbaa !2
  %tmp164 = getelementptr inbounds i32, i32* %arg2, i64 %tmp161
  store i32 %tmp163, i32* %tmp164, align 4, !tbaa !2
  %tmp165 = or i64 %tmp137, 499
  %tmp166 = getelementptr inbounds i32, i32* %arg1, i64 %tmp165
  %tmp167 = load i32, i32* %tmp166, align 4, !tbaa !2
  %tmp168 = getelementptr inbounds i32, i32* %arg2, i64 %tmp165
  store i32 %tmp167, i32* %tmp168, align 4, !tbaa !2
  %tmp169 = or i64 %tmp137, 4
  %tmp170 = getelementptr inbounds i32, i32* %arg1, i64 %tmp169
  %tmp171 = load i32, i32* %tmp170, align 4, !tbaa !2
  %tmp172 = getelementptr inbounds i32, i32* %arg2, i64 %tmp169
  store i32 %tmp171, i32* %tmp172, align 4, !tbaa !2
  %tmp173 = or i64 %tmp137, 500
  %tmp174 = getelementptr inbounds i32, i32* %arg1, i64 %tmp173
  %tmp175 = load i32, i32* %tmp174, align 4, !tbaa !2
  %tmp176 = getelementptr inbounds i32, i32* %arg2, i64 %tmp173
  store i32 %tmp175, i32* %tmp176, align 4, !tbaa !2
  %tmp177 = or i64 %tmp137, 5
  %tmp178 = getelementptr inbounds i32, i32* %arg1, i64 %tmp177
  %tmp179 = load i32, i32* %tmp178, align 4, !tbaa !2
  %tmp180 = getelementptr inbounds i32, i32* %arg2, i64 %tmp177
  store i32 %tmp179, i32* %tmp180, align 4, !tbaa !2
  %tmp181 = or i64 %tmp137, 501
  %tmp182 = getelementptr inbounds i32, i32* %arg1, i64 %tmp181
  %tmp183 = load i32, i32* %tmp182, align 4, !tbaa !2
  %tmp184 = getelementptr inbounds i32, i32* %arg2, i64 %tmp181
  store i32 %tmp183, i32* %tmp184, align 4, !tbaa !2
  %tmp185 = or i64 %tmp137, 6
  %tmp186 = getelementptr inbounds i32, i32* %arg1, i64 %tmp185
  %tmp187 = load i32, i32* %tmp186, align 4, !tbaa !2
  %tmp188 = getelementptr inbounds i32, i32* %arg2, i64 %tmp185
  store i32 %tmp187, i32* %tmp188, align 4, !tbaa !2
  %tmp189 = or i64 %tmp137, 502
  %tmp190 = getelementptr inbounds i32, i32* %arg1, i64 %tmp189
  %tmp191 = load i32, i32* %tmp190, align 4, !tbaa !2
  %tmp192 = getelementptr inbounds i32, i32* %arg2, i64 %tmp189
  store i32 %tmp191, i32* %tmp192, align 4, !tbaa !2
  %tmp193 = or i64 %tmp137, 7
  %tmp194 = getelementptr inbounds i32, i32* %arg1, i64 %tmp193
  %tmp195 = load i32, i32* %tmp194, align 4, !tbaa !2
  %tmp196 = getelementptr inbounds i32, i32* %arg2, i64 %tmp193
  store i32 %tmp195, i32* %tmp196, align 4, !tbaa !2
  %tmp197 = or i64 %tmp137, 503
  %tmp198 = getelementptr inbounds i32, i32* %arg1, i64 %tmp197
  %tmp199 = load i32, i32* %tmp198, align 4, !tbaa !2
  %tmp200 = getelementptr inbounds i32, i32* %arg2, i64 %tmp197
  store i32 %tmp199, i32* %tmp200, align 4, !tbaa !2
  %tmp201 = or i64 %tmp137, 8
  %tmp202 = getelementptr inbounds i32, i32* %arg1, i64 %tmp201
  %tmp203 = load i32, i32* %tmp202, align 4, !tbaa !2
  %tmp204 = getelementptr inbounds i32, i32* %arg2, i64 %tmp201
  store i32 %tmp203, i32* %tmp204, align 4, !tbaa !2
  %tmp205 = or i64 %tmp137, 504
  %tmp206 = getelementptr inbounds i32, i32* %arg1, i64 %tmp205
  %tmp207 = load i32, i32* %tmp206, align 4, !tbaa !2
  %tmp208 = getelementptr inbounds i32, i32* %arg2, i64 %tmp205
  store i32 %tmp207, i32* %tmp208, align 4, !tbaa !2
  %tmp209 = or i64 %tmp137, 9
  %tmp210 = getelementptr inbounds i32, i32* %arg1, i64 %tmp209
  %tmp211 = load i32, i32* %tmp210, align 4, !tbaa !2
  %tmp212 = getelementptr inbounds i32, i32* %arg2, i64 %tmp209
  store i32 %tmp211, i32* %tmp212, align 4, !tbaa !2
  %tmp213 = or i64 %tmp137, 505
  %tmp214 = getelementptr inbounds i32, i32* %arg1, i64 %tmp213
  %tmp215 = load i32, i32* %tmp214, align 4, !tbaa !2
  %tmp216 = getelementptr inbounds i32, i32* %arg2, i64 %tmp213
  store i32 %tmp215, i32* %tmp216, align 4, !tbaa !2
  %tmp217 = or i64 %tmp137, 10
  %tmp218 = getelementptr inbounds i32, i32* %arg1, i64 %tmp217
  %tmp219 = load i32, i32* %tmp218, align 4, !tbaa !2
  %tmp220 = getelementptr inbounds i32, i32* %arg2, i64 %tmp217
  store i32 %tmp219, i32* %tmp220, align 4, !tbaa !2
  %tmp221 = or i64 %tmp137, 506
  %tmp222 = getelementptr inbounds i32, i32* %arg1, i64 %tmp221
  %tmp223 = load i32, i32* %tmp222, align 4, !tbaa !2
  %tmp224 = getelementptr inbounds i32, i32* %arg2, i64 %tmp221
  store i32 %tmp223, i32* %tmp224, align 4, !tbaa !2
  %tmp225 = or i64 %tmp137, 11
  %tmp226 = getelementptr inbounds i32, i32* %arg1, i64 %tmp225
  %tmp227 = load i32, i32* %tmp226, align 4, !tbaa !2
  %tmp228 = getelementptr inbounds i32, i32* %arg2, i64 %tmp225
  store i32 %tmp227, i32* %tmp228, align 4, !tbaa !2
  %tmp229 = or i64 %tmp137, 507
  %tmp230 = getelementptr inbounds i32, i32* %arg1, i64 %tmp229
  %tmp231 = load i32, i32* %tmp230, align 4, !tbaa !2
  %tmp232 = getelementptr inbounds i32, i32* %arg2, i64 %tmp229
  store i32 %tmp231, i32* %tmp232, align 4, !tbaa !2
  %tmp233 = or i64 %tmp137, 12
  %tmp234 = getelementptr inbounds i32, i32* %arg1, i64 %tmp233
  %tmp235 = load i32, i32* %tmp234, align 4, !tbaa !2
  %tmp236 = getelementptr inbounds i32, i32* %arg2, i64 %tmp233
  store i32 %tmp235, i32* %tmp236, align 4, !tbaa !2
  %tmp237 = or i64 %tmp137, 508
  %tmp238 = getelementptr inbounds i32, i32* %arg1, i64 %tmp237
  %tmp239 = load i32, i32* %tmp238, align 4, !tbaa !2
  %tmp240 = getelementptr inbounds i32, i32* %arg2, i64 %tmp237
  store i32 %tmp239, i32* %tmp240, align 4, !tbaa !2
  %tmp241 = or i64 %tmp137, 13
  %tmp242 = getelementptr inbounds i32, i32* %arg1, i64 %tmp241
  %tmp243 = load i32, i32* %tmp242, align 4, !tbaa !2
  %tmp244 = getelementptr inbounds i32, i32* %arg2, i64 %tmp241
  store i32 %tmp243, i32* %tmp244, align 4, !tbaa !2
  %tmp245 = or i64 %tmp137, 509
  %tmp246 = getelementptr inbounds i32, i32* %arg1, i64 %tmp245
  %tmp247 = load i32, i32* %tmp246, align 4, !tbaa !2
  %tmp248 = getelementptr inbounds i32, i32* %arg2, i64 %tmp245
  store i32 %tmp247, i32* %tmp248, align 4, !tbaa !2
  %tmp249 = or i64 %tmp137, 14
  %tmp250 = getelementptr inbounds i32, i32* %arg1, i64 %tmp249
  %tmp251 = load i32, i32* %tmp250, align 4, !tbaa !2
  %tmp252 = getelementptr inbounds i32, i32* %arg2, i64 %tmp249
  store i32 %tmp251, i32* %tmp252, align 4, !tbaa !2
  %tmp253 = or i64 %tmp137, 510
  %tmp254 = getelementptr inbounds i32, i32* %arg1, i64 %tmp253
  %tmp255 = load i32, i32* %tmp254, align 4, !tbaa !2
  %tmp256 = getelementptr inbounds i32, i32* %arg2, i64 %tmp253
  store i32 %tmp255, i32* %tmp256, align 4, !tbaa !2
  %tmp257 = or i64 %tmp137, 15
  %tmp258 = getelementptr inbounds i32, i32* %arg1, i64 %tmp257
  %tmp259 = load i32, i32* %tmp258, align 4, !tbaa !2
  %tmp260 = getelementptr inbounds i32, i32* %arg2, i64 %tmp257
  store i32 %tmp259, i32* %tmp260, align 4, !tbaa !2
  %tmp261 = or i64 %tmp137, 511
  %tmp262 = getelementptr inbounds i32, i32* %arg1, i64 %tmp261
  %tmp263 = load i32, i32* %tmp262, align 4, !tbaa !2
  %tmp264 = getelementptr inbounds i32, i32* %arg2, i64 %tmp261
  store i32 %tmp263, i32* %tmp264, align 4, !tbaa !2
  %tmp265 = add nuw nsw i64 %tmp136, 1
  %tmp266 = icmp eq i64 %tmp265, 31
  br i1 %tmp266, label %bb267, label %bb135

bb267:                                            ; preds = %bb135
  br label %bb268

bb268:                                            ; preds = %bb294, %bb267
  %tmp269 = phi i64 [ %tmp295, %bb294 ], [ 1, %bb267 ]
  %tmp270 = shl i64 %tmp269, 5
  br label %bb271

bb271:                                            ; preds = %bb271, %bb268
  %tmp272 = phi i64 [ 1, %bb268 ], [ %tmp292, %bb271 ]
  %tmp273 = add nuw nsw i64 %tmp272, %tmp270
  %tmp274 = shl nsw i64 %tmp273, 4
  %tmp275 = getelementptr inbounds i32, i32* %arg1, i64 %tmp274
  %tmp276 = load i32, i32* %tmp275, align 4, !tbaa !2
  %tmp277 = getelementptr inbounds i32, i32* %arg2, i64 %tmp274
  store i32 %tmp276, i32* %tmp277, align 4, !tbaa !2
  %tmp278 = or i64 %tmp274, 15
  %tmp279 = getelementptr inbounds i32, i32* %arg1, i64 %tmp278
  %tmp280 = load i32, i32* %tmp279, align 4, !tbaa !2
  %tmp281 = getelementptr inbounds i32, i32* %arg2, i64 %tmp278
  store i32 %tmp280, i32* %tmp281, align 4, !tbaa !2
  %tmp282 = add nuw nsw i64 %tmp272, 1
  %tmp283 = add nuw nsw i64 %tmp282, %tmp270
  %tmp284 = shl nsw i64 %tmp283, 4
  %tmp285 = getelementptr inbounds i32, i32* %arg1, i64 %tmp284
  %tmp286 = load i32, i32* %tmp285, align 4, !tbaa !2
  %tmp287 = getelementptr inbounds i32, i32* %arg2, i64 %tmp284
  store i32 %tmp286, i32* %tmp287, align 4, !tbaa !2
  %tmp288 = or i64 %tmp284, 15
  %tmp289 = getelementptr inbounds i32, i32* %arg1, i64 %tmp288
  %tmp290 = load i32, i32* %tmp289, align 4, !tbaa !2
  %tmp291 = getelementptr inbounds i32, i32* %arg2, i64 %tmp288
  store i32 %tmp290, i32* %tmp291, align 4, !tbaa !2
  %tmp292 = add nuw nsw i64 %tmp272, 2
  %tmp293 = icmp eq i64 %tmp292, 31
  br i1 %tmp293, label %bb294, label %bb271

bb294:                                            ; preds = %bb271
  %tmp295 = add nuw nsw i64 %tmp269, 1
  %tmp296 = icmp eq i64 %tmp295, 31
  br i1 %tmp296, label %bb297, label %bb268

bb297:                                            ; preds = %bb294
  %tmp298 = getelementptr inbounds i32, i32* %arg, i64 1
  br label %bb299

bb299:                                            ; preds = %bb349, %bb297
  %tmp300 = phi i64 [ 1, %bb297 ], [ %tmp350, %bb349 ]
  %tmp301 = shl i64 %tmp300, 5
  br label %bb302

bb302:                                            ; preds = %bb346, %bb299
  %tmp303 = phi i64 [ 1, %bb299 ], [ %tmp347, %bb346 ]
  %tmp304 = add nuw nsw i64 %tmp303, %tmp301
  %tmp305 = shl i64 %tmp304, 4
  %tmp306 = add nuw nsw i64 %tmp305, 512
  %tmp307 = add nsw i64 %tmp305, -512
  %tmp308 = add nuw nsw i64 %tmp305, 16
  %tmp309 = add nsw i64 %tmp305, -16
  br label %bb310

bb310:                                            ; preds = %bb310, %bb302
  %tmp311 = phi i64 [ 1, %bb302 ], [ %tmp330, %bb310 ]
  %tmp312 = add nuw nsw i64 %tmp311, %tmp305
  %tmp313 = getelementptr inbounds i32, i32* %arg1, i64 %tmp312
  %tmp314 = load i32, i32* %tmp313, align 4, !tbaa !2
  %tmp315 = add nuw nsw i64 %tmp311, %tmp306
  %tmp316 = getelementptr inbounds i32, i32* %arg1, i64 %tmp315
  %tmp317 = load i32, i32* %tmp316, align 4, !tbaa !2
  %tmp318 = add nuw nsw i64 %tmp311, %tmp307
  %tmp319 = getelementptr inbounds i32, i32* %arg1, i64 %tmp318
  %tmp320 = load i32, i32* %tmp319, align 4, !tbaa !2
  %tmp321 = add nsw i32 %tmp320, %tmp317
  %tmp322 = add nuw nsw i64 %tmp311, %tmp308
  %tmp323 = getelementptr inbounds i32, i32* %arg1, i64 %tmp322
  %tmp324 = load i32, i32* %tmp323, align 4, !tbaa !2
  %tmp325 = add nsw i32 %tmp321, %tmp324
  %tmp326 = add nuw nsw i64 %tmp311, %tmp309
  %tmp327 = getelementptr inbounds i32, i32* %arg1, i64 %tmp326
  %tmp328 = load i32, i32* %tmp327, align 4, !tbaa !2
  %tmp329 = add nsw i32 %tmp325, %tmp328
  %tmp330 = add nuw nsw i64 %tmp311, 1
  %tmp331 = add nuw nsw i64 %tmp330, %tmp305
  %tmp332 = getelementptr inbounds i32, i32* %arg1, i64 %tmp331
  %tmp333 = load i32, i32* %tmp332, align 4, !tbaa !2
  %tmp334 = add nsw i32 %tmp329, %tmp333
  %tmp335 = add nsw i64 %tmp312, -1
  %tmp336 = getelementptr inbounds i32, i32* %arg1, i64 %tmp335
  %tmp337 = load i32, i32* %tmp336, align 4, !tbaa !2
  %tmp338 = add nsw i32 %tmp334, %tmp337
  %tmp339 = load i32, i32* %arg, align 4, !tbaa !2
  %tmp340 = mul nsw i32 %tmp339, %tmp314
  %tmp341 = load i32, i32* %tmp298, align 4, !tbaa !2
  %tmp342 = mul nsw i32 %tmp341, %tmp338
  %tmp343 = add nsw i32 %tmp342, %tmp340
  %tmp344 = getelementptr inbounds i32, i32* %arg2, i64 %tmp312
  store i32 %tmp343, i32* %tmp344, align 4, !tbaa !2
  %tmp345 = icmp eq i64 %tmp330, 15
  br i1 %tmp345, label %bb346, label %bb310

bb346:                                            ; preds = %bb310
  %tmp347 = add nuw nsw i64 %tmp303, 1
  %tmp348 = icmp eq i64 %tmp347, 31
  br i1 %tmp348, label %bb349, label %bb302

bb349:                                            ; preds = %bb346
  %tmp350 = add nuw nsw i64 %tmp300, 1
  %tmp351 = icmp eq i64 %tmp350, 31
  br i1 %tmp351, label %bb352, label %bb299

bb352:                                            ; preds = %bb349
  ret void
}

; Function Attrs: noinline norecurse nounwind uwtable
define internal dso_local void @stencil3d(i32* nocapture readonly %arg, i32* nocapture readonly %arg1, i32* nocapture %arg2) #0 {
bb:
  tail call void @kernel(i32* %arg, i32* %arg1, i32* %arg2)
  tail call void @kernel(i32* %arg, i32* %arg1, i32* %arg2)
  ret void
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @run_benchmark(i8* %arg) #1 {
bb:
  %tmp = bitcast i8* %arg to i32*
  %tmp1 = getelementptr inbounds i8, i8* %arg, i64 8
  %tmp2 = bitcast i8* %tmp1 to i32*
  %tmp3 = getelementptr inbounds i8, i8* %arg, i64 65544
  %tmp4 = bitcast i8* %tmp3 to i32*
  tail call void @stencil3d(i32* %tmp, i32* nonnull %tmp2, i32* nonnull %tmp4) #8
  ret void
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @input_to_data(i32 %arg, i8* %arg1) #1 {
bb:
  %tmp = tail call i8* @readfile(i32 %arg) #8
  %tmp2 = tail call i8* @find_section_start(i8* %tmp, i32 1) #8
  %tmp3 = bitcast i8* %arg1 to i32*
  %tmp4 = tail call i32 @parse_int32_t_array(i8* %tmp2, i32* %tmp3, i32 2) #8
  %tmp5 = tail call i8* @find_section_start(i8* %tmp, i32 2) #8
  %tmp6 = getelementptr inbounds i8, i8* %arg1, i64 8
  %tmp7 = bitcast i8* %tmp6 to i32*
  %tmp8 = tail call i32 @parse_int32_t_array(i8* %tmp5, i32* nonnull %tmp7, i32 16384) #8
  tail call void @free(i8* %tmp) #8
  ret void
}

; Function Attrs: nounwind
declare void @free(i8* nocapture) local_unnamed_addr #2

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @data_to_input(i32 %arg, i8* %arg1) #1 {
bb:
  %tmp = tail call i32 @write_section_header(i32 %arg) #8
  %tmp2 = bitcast i8* %arg1 to i32*
  %tmp3 = tail call i32 @write_int32_t_array(i32 %arg, i32* %tmp2, i32 2) #8
  %tmp4 = tail call i32 @write_section_header(i32 %arg) #8
  %tmp5 = getelementptr inbounds i8, i8* %arg1, i64 8
  %tmp6 = bitcast i8* %tmp5 to i32*
  %tmp7 = tail call i32 @write_int32_t_array(i32 %arg, i32* nonnull %tmp6, i32 16384) #8
  ret void
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @output_to_data(i32 %arg, i8* %arg1) #1 {
bb:
  %tmp = tail call i8* @readfile(i32 %arg) #8
  %tmp2 = tail call i8* @find_section_start(i8* %tmp, i32 1) #8
  %tmp3 = getelementptr inbounds i8, i8* %arg1, i64 65544
  %tmp4 = bitcast i8* %tmp3 to i32*
  %tmp5 = tail call i32 @parse_int32_t_array(i8* %tmp2, i32* nonnull %tmp4, i32 16384) #8
  tail call void @free(i8* %tmp) #8
  ret void
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @data_to_output(i32 %arg, i8* %arg1) #1 {
bb:
  %tmp = tail call i32 @write_section_header(i32 %arg) #8
  %tmp2 = getelementptr inbounds i8, i8* %arg1, i64 65544
  %tmp3 = bitcast i8* %tmp2 to i32*
  %tmp4 = tail call i32 @write_int32_t_array(i32 %arg, i32* nonnull %tmp3, i32 16384) #8
  ret void
}

; Function Attrs: noinline norecurse nounwind readonly uwtable
define internal dso_local i32 @check_data(i8* nocapture readonly %arg, i8* nocapture readonly %arg1) #3 {
bb:
  %tmp = getelementptr inbounds i8, i8* %arg, i64 65544
  %tmp2 = bitcast i8* %tmp to [16384 x i32]*
  %tmp3 = getelementptr inbounds i8, i8* %arg1, i64 65544
  %tmp4 = bitcast i8* %tmp3 to [16384 x i32]*
  br label %bb5

bb5:                                              ; preds = %bb5, %bb
  %tmp6 = phi i64 [ 0, %bb ], [ %tmp46, %bb5 ]
  %tmp7 = phi <4 x i32> [ zeroinitializer, %bb ], [ %tmp44, %bb5 ]
  %tmp8 = phi <4 x i32> [ zeroinitializer, %bb ], [ %tmp45, %bb5 ]
  %tmp9 = getelementptr inbounds [16384 x i32], [16384 x i32]* %tmp2, i64 0, i64 %tmp6
  %tmp10 = bitcast i32* %tmp9 to <4 x i32>*
  %tmp11 = load <4 x i32>, <4 x i32>* %tmp10, align 4, !tbaa !2
  %tmp12 = getelementptr i32, i32* %tmp9, i64 4
  %tmp13 = bitcast i32* %tmp12 to <4 x i32>*
  %tmp14 = load <4 x i32>, <4 x i32>* %tmp13, align 4, !tbaa !2
  %tmp15 = getelementptr inbounds [16384 x i32], [16384 x i32]* %tmp4, i64 0, i64 %tmp6
  %tmp16 = bitcast i32* %tmp15 to <4 x i32>*
  %tmp17 = load <4 x i32>, <4 x i32>* %tmp16, align 4, !tbaa !2
  %tmp18 = getelementptr i32, i32* %tmp15, i64 4
  %tmp19 = bitcast i32* %tmp18 to <4 x i32>*
  %tmp20 = load <4 x i32>, <4 x i32>* %tmp19, align 4, !tbaa !2
  %tmp21 = icmp ne <4 x i32> %tmp11, %tmp17
  %tmp22 = icmp ne <4 x i32> %tmp14, %tmp20
  %tmp23 = zext <4 x i1> %tmp21 to <4 x i32>
  %tmp24 = zext <4 x i1> %tmp22 to <4 x i32>
  %tmp25 = or <4 x i32> %tmp7, %tmp23
  %tmp26 = or <4 x i32> %tmp8, %tmp24
  %tmp27 = or i64 %tmp6, 8
  %tmp28 = getelementptr inbounds [16384 x i32], [16384 x i32]* %tmp2, i64 0, i64 %tmp27
  %tmp29 = bitcast i32* %tmp28 to <4 x i32>*
  %tmp30 = load <4 x i32>, <4 x i32>* %tmp29, align 4, !tbaa !2
  %tmp31 = getelementptr i32, i32* %tmp28, i64 4
  %tmp32 = bitcast i32* %tmp31 to <4 x i32>*
  %tmp33 = load <4 x i32>, <4 x i32>* %tmp32, align 4, !tbaa !2
  %tmp34 = getelementptr inbounds [16384 x i32], [16384 x i32]* %tmp4, i64 0, i64 %tmp27
  %tmp35 = bitcast i32* %tmp34 to <4 x i32>*
  %tmp36 = load <4 x i32>, <4 x i32>* %tmp35, align 4, !tbaa !2
  %tmp37 = getelementptr i32, i32* %tmp34, i64 4
  %tmp38 = bitcast i32* %tmp37 to <4 x i32>*
  %tmp39 = load <4 x i32>, <4 x i32>* %tmp38, align 4, !tbaa !2
  %tmp40 = icmp ne <4 x i32> %tmp30, %tmp36
  %tmp41 = icmp ne <4 x i32> %tmp33, %tmp39
  %tmp42 = zext <4 x i1> %tmp40 to <4 x i32>
  %tmp43 = zext <4 x i1> %tmp41 to <4 x i32>
  %tmp44 = or <4 x i32> %tmp25, %tmp42
  %tmp45 = or <4 x i32> %tmp26, %tmp43
  %tmp46 = add nuw nsw i64 %tmp6, 16
  %tmp47 = icmp eq i64 %tmp46, 16384
  br i1 %tmp47, label %bb48, label %bb5, !llvm.loop !6

bb48:                                             ; preds = %bb5
  %tmp49 = or <4 x i32> %tmp45, %tmp44
  %tmp50 = shufflevector <4 x i32> %tmp49, <4 x i32> undef, <4 x i32> <i32 2, i32 3, i32 undef, i32 undef>
  %tmp51 = or <4 x i32> %tmp49, %tmp50
  %tmp52 = shufflevector <4 x i32> %tmp51, <4 x i32> undef, <4 x i32> <i32 1, i32 undef, i32 undef, i32 undef>
  %tmp53 = or <4 x i32> %tmp51, %tmp52
  %tmp54 = extractelement <4 x i32> %tmp53, i32 0
  %tmp55 = icmp eq i32 %tmp54, 0
  %tmp56 = zext i1 %tmp55 to i32
  ret i32 %tmp56
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local noalias i8* @readfile(i32 %arg) #1 {
bb:
  %tmp = alloca %struct.stat, align 8
  %tmp1 = bitcast %struct.stat* %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 144, i8* nonnull %tmp1) #8
  %tmp2 = icmp sgt i32 %arg, 1
  br i1 %tmp2, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 40, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #9
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = call i32 @fstat(i32 %arg, %struct.stat* %tmp) #8
  %tmp6 = icmp eq i32 %tmp5, 0
  br i1 %tmp6, label %bb8, label %bb7

bb7:                                              ; preds = %bb4
  call void @__assert_fail(i8* getelementptr inbounds ([51 x i8], [51 x i8]* @.str.4, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 41, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #9
  unreachable

bb8:                                              ; preds = %bb4
  %tmp9 = getelementptr inbounds %struct.stat, %struct.stat* %tmp, i64 0, i32 8
  %tmp10 = load i64, i64* %tmp9, align 8, !tbaa !8
  %tmp11 = icmp sgt i64 %tmp10, 0
  br i1 %tmp11, label %bb13, label %bb12

bb12:                                             ; preds = %bb8
  call void @__assert_fail(i8* getelementptr inbounds ([25 x i8], [25 x i8]* @.str.6, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 43, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #9
  unreachable

bb13:                                             ; preds = %bb8
  %tmp14 = add nsw i64 %tmp10, 1
  %tmp15 = call noalias i8* @malloc(i64 %tmp14) #8
  br label %bb18

bb16:                                             ; preds = %bb18
  %tmp17 = icmp sgt i64 %tmp10, %tmp24
  br i1 %tmp17, label %bb18, label %bb26

bb18:                                             ; preds = %bb16, %bb13
  %tmp19 = phi i64 [ 0, %bb13 ], [ %tmp24, %bb16 ]
  %tmp20 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp19
  %tmp21 = sub nsw i64 %tmp10, %tmp19
  %tmp22 = call i64 @read(i32 %arg, i8* %tmp20, i64 %tmp21) #8
  %tmp23 = icmp sgt i64 %tmp22, -1
  %tmp24 = add nsw i64 %tmp22, %tmp19
  br i1 %tmp23, label %bb16, label %bb25

bb25:                                             ; preds = %bb18
  call void @__assert_fail(i8* getelementptr inbounds ([29 x i8], [29 x i8]* @.str.8, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 48, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #9
  unreachable

bb26:                                             ; preds = %bb16
  %tmp27 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp10
  store i8 0, i8* %tmp27, align 1, !tbaa !12
  %tmp28 = call i32 @close(i32 %arg) #8
  call void @llvm.lifetime.end.p0i8(i64 144, i8* nonnull %tmp1) #8
  ret i8* %tmp15
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64, i8* nocapture) #4

; Function Attrs: noreturn nounwind
declare void @__assert_fail(i8*, i8*, i32, i8*) local_unnamed_addr #5

; Function Attrs: noinline nounwind uwtable
define available_externally dso_local i32 @fstat(i32 %arg, %struct.stat* nonnull %arg1) local_unnamed_addr #1 {
bb:
  %tmp = tail call i32 @__fxstat(i32 1, i32 %arg, %struct.stat* nonnull %arg1) #8
  ret i32 %tmp
}

; Function Attrs: nounwind
declare noalias i8* @malloc(i64) local_unnamed_addr #2

declare i64 @read(i32, i8* nocapture, i64) local_unnamed_addr #6

declare i32 @close(i32) local_unnamed_addr #6

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64, i8* nocapture) #4

; Function Attrs: nounwind
declare i32 @__fxstat(i32, i32, %struct.stat*) local_unnamed_addr #2

; Function Attrs: noinline nounwind uwtable
define internal dso_local i8* @find_section_start(i8* readonly %arg, i32 %arg1) #1 {
bb:
  %tmp = icmp sgt i32 %arg1, -1
  br i1 %tmp, label %bb3, label %bb2

bb2:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([33 x i8], [33 x i8]* @.str.10, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 59, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.find_section_start, i64 0, i64 0)) #9
  unreachable

bb3:                                              ; preds = %bb
  %tmp4 = icmp eq i32 %arg1, 0
  br i1 %tmp4, label %bb34, label %bb5

bb5:                                              ; preds = %bb3
  %tmp6 = load i8, i8* %arg, align 1, !tbaa !12
  br label %bb7

bb7:                                              ; preds = %bb24, %bb5
  %tmp8 = phi i8 [ %tmp6, %bb5 ], [ %tmp26, %bb24 ]
  %tmp9 = phi i32 [ 0, %bb5 ], [ %tmp27, %bb24 ]
  %tmp10 = phi i8* [ %arg, %bb5 ], [ %tmp25, %bb24 ]
  switch i8 %tmp8, label %bb11 [
    i8 0, label %bb32
    i8 37, label %bb14
  ]

bb11:                                             ; preds = %bb7
  %tmp12 = getelementptr inbounds i8, i8* %tmp10, i64 1
  %tmp13 = load i8, i8* %tmp12, align 1, !tbaa !12
  br label %bb24

bb14:                                             ; preds = %bb7
  %tmp15 = getelementptr inbounds i8, i8* %tmp10, i64 1
  %tmp16 = load i8, i8* %tmp15, align 1, !tbaa !12
  %tmp17 = icmp eq i8 %tmp16, 37
  br i1 %tmp17, label %bb18, label %bb24

bb18:                                             ; preds = %bb14
  %tmp19 = getelementptr inbounds i8, i8* %tmp10, i64 2
  %tmp20 = load i8, i8* %tmp19, align 1, !tbaa !12
  %tmp21 = icmp eq i8 %tmp20, 10
  %tmp22 = zext i1 %tmp21 to i32
  %tmp23 = add nsw i32 %tmp9, %tmp22
  br label %bb24

bb24:                                             ; preds = %bb18, %bb14, %bb11
  %tmp25 = phi i8* [ %tmp12, %bb11 ], [ %tmp15, %bb18 ], [ %tmp15, %bb14 ]
  %tmp26 = phi i8 [ %tmp13, %bb11 ], [ 37, %bb18 ], [ %tmp16, %bb14 ]
  %tmp27 = phi i32 [ %tmp9, %bb11 ], [ %tmp23, %bb18 ], [ %tmp9, %bb14 ]
  %tmp28 = icmp slt i32 %tmp27, %arg1
  br i1 %tmp28, label %bb7, label %bb29

bb29:                                             ; preds = %bb24
  %tmp30 = icmp eq i8 %tmp26, 0
  %tmp31 = getelementptr inbounds i8, i8* %tmp10, i64 3
  br i1 %tmp30, label %bb32, label %bb34

bb32:                                             ; preds = %bb29, %bb7
  %tmp33 = phi i8* [ %tmp25, %bb29 ], [ %tmp10, %bb7 ]
  br label %bb34

bb34:                                             ; preds = %bb32, %bb29, %bb3
  %tmp35 = phi i8* [ %tmp33, %bb32 ], [ %tmp31, %bb29 ], [ %arg, %bb3 ]
  ret i8* %tmp35
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_string(i8* readonly %arg, i8* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp eq i8* %arg, null
  br i1 %tmp, label %bb3, label %bb4

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 79, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.parse_string, i64 0, i64 0)) #9
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp slt i32 %arg2, 0
  br i1 %tmp5, label %bb8, label %bb6

bb6:                                              ; preds = %bb4
  %tmp7 = sext i32 %arg2 to i64
  tail call void @llvm.memcpy.p0i8.p0i8.i64(i8* %arg1, i8* nonnull %arg, i64 %tmp7, i32 1, i1 false)
  br label %bb47

bb8:                                              ; preds = %bb4
  %tmp9 = load i8, i8* %arg, align 1, !tbaa !12
  %tmp10 = icmp eq i8 %tmp9, 0
  br i1 %tmp10, label %bb43, label %bb11

bb11:                                             ; preds = %bb8
  %tmp12 = getelementptr inbounds i8, i8* %arg, i64 1
  %tmp13 = load i8, i8* %tmp12, align 1, !tbaa !12
  br label %bb17

bb14:                                             ; preds = %bb31
  %tmp15 = load i8, i8* %tmp24, align 1, !tbaa !12
  %tmp16 = icmp eq i8 %tmp15, 0
  br i1 %tmp16, label %bb43, label %bb17

bb17:                                             ; preds = %bb14, %bb11
  %tmp18 = phi i8 [ %tmp13, %bb11 ], [ %tmp29, %bb14 ]
  %tmp19 = phi i64 [ 0, %bb11 ], [ %tmp22, %bb14 ]
  %tmp20 = phi i8 [ %tmp9, %bb11 ], [ %tmp15, %bb14 ]
  %tmp21 = phi i32 [ 0, %bb11 ], [ %tmp23, %bb14 ]
  %tmp22 = add nuw i64 %tmp19, 1
  %tmp23 = add nuw nsw i32 %tmp21, 1
  %tmp24 = getelementptr inbounds i8, i8* %arg, i64 %tmp22
  %tmp25 = icmp eq i8 %tmp18, 0
  br i1 %tmp25, label %bb37, label %bb26

bb26:                                             ; preds = %bb17
  %tmp27 = add nuw nsw i64 %tmp19, 2
  %tmp28 = getelementptr inbounds i8, i8* %arg, i64 %tmp27
  %tmp29 = load i8, i8* %tmp28, align 1, !tbaa !12
  %tmp30 = icmp eq i8 %tmp29, 0
  br i1 %tmp30, label %bb39, label %bb31

bb31:                                             ; preds = %bb26
  %tmp32 = icmp eq i8 %tmp20, 10
  %tmp33 = icmp eq i8 %tmp18, 37
  %tmp34 = and i1 %tmp32, %tmp33
  %tmp35 = icmp eq i8 %tmp29, 37
  %tmp36 = and i1 %tmp34, %tmp35
  br i1 %tmp36, label %bb41, label %bb14

bb37:                                             ; preds = %bb17
  %tmp38 = trunc i64 %tmp19 to i32
  br label %bb43

bb39:                                             ; preds = %bb26
  %tmp40 = trunc i64 %tmp19 to i32
  br label %bb43

bb41:                                             ; preds = %bb31
  %tmp42 = trunc i64 %tmp19 to i32
  br label %bb43

bb43:                                             ; preds = %bb41, %bb39, %bb37, %bb14, %bb8
  %tmp44 = phi i32 [ 0, %bb8 ], [ %tmp38, %bb37 ], [ %tmp40, %bb39 ], [ %tmp42, %bb41 ], [ %tmp23, %bb14 ]
  %tmp45 = zext i32 %tmp44 to i64
  tail call void @llvm.memcpy.p0i8.p0i8.i64(i8* %arg1, i8* nonnull %arg, i64 %tmp45, i32 1, i1 false)
  %tmp46 = getelementptr inbounds i8, i8* %arg1, i64 %tmp45
  store i8 0, i8* %tmp46, align 1, !tbaa !12
  br label %bb47

bb47:                                             ; preds = %bb43, %bb6
  ret i32 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture writeonly, i8* nocapture readonly, i64, i32, i1) #4

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_uint8_t_array(i8* %arg, i8* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #8
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 132, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_uint8_t_array, i64 0, i64 0)) #9
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp8 = icmp ne i8* %tmp7, null
  %tmp9 = icmp sgt i32 %arg2, 0
  %tmp10 = and i1 %tmp9, %tmp8
  br i1 %tmp10, label %bb11, label %bb34

bb11:                                             ; preds = %bb6
  %tmp12 = sext i32 %arg2 to i64
  br label %bb13

bb13:                                             ; preds = %bb25, %bb11
  %tmp14 = phi i64 [ 0, %bb11 ], [ %tmp27, %bb25 ]
  %tmp15 = phi i8* [ %tmp7, %bb11 ], [ %tmp30, %bb25 ]
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !13
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = trunc i64 %tmp16 to i8
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !13
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !12
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !13
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #10
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i8, i8* %arg1, i64 %tmp14
  store i8 %tmp17, i8* %tmp26, align 1, !tbaa !12
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !12
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #11
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !12
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #8
  ret i32 0
}

; Function Attrs: nounwind
declare i8* @strtok(i8*, i8* nocapture readonly) local_unnamed_addr #2

; Function Attrs: nounwind
declare i64 @strtol(i8* readonly, i8** nocapture, i32) local_unnamed_addr #2

; Function Attrs: nounwind
declare i32 @fprintf(%struct._IO_FILE* nocapture, i8* nocapture readonly, ...) local_unnamed_addr #2

; Function Attrs: argmemonly nounwind readonly
declare i64 @strlen(i8* nocapture) local_unnamed_addr #7

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_uint16_t_array(i8* %arg, i16* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #8
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 133, i8* getelementptr inbounds ([50 x i8], [50 x i8]* @__PRETTY_FUNCTION__.parse_uint16_t_array, i64 0, i64 0)) #9
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp8 = icmp ne i8* %tmp7, null
  %tmp9 = icmp sgt i32 %arg2, 0
  %tmp10 = and i1 %tmp9, %tmp8
  br i1 %tmp10, label %bb11, label %bb34

bb11:                                             ; preds = %bb6
  %tmp12 = sext i32 %arg2 to i64
  br label %bb13

bb13:                                             ; preds = %bb25, %bb11
  %tmp14 = phi i64 [ 0, %bb11 ], [ %tmp27, %bb25 ]
  %tmp15 = phi i8* [ %tmp7, %bb11 ], [ %tmp30, %bb25 ]
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !13
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = trunc i64 %tmp16 to i16
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !13
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !12
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !13
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #10
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i16, i16* %arg1, i64 %tmp14
  store i16 %tmp17, i16* %tmp26, align 2, !tbaa !15
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !12
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #11
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !12
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #8
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_uint32_t_array(i8* %arg, i32* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #8
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 134, i8* getelementptr inbounds ([50 x i8], [50 x i8]* @__PRETTY_FUNCTION__.parse_uint32_t_array, i64 0, i64 0)) #9
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp8 = icmp ne i8* %tmp7, null
  %tmp9 = icmp sgt i32 %arg2, 0
  %tmp10 = and i1 %tmp9, %tmp8
  br i1 %tmp10, label %bb11, label %bb34

bb11:                                             ; preds = %bb6
  %tmp12 = sext i32 %arg2 to i64
  br label %bb13

bb13:                                             ; preds = %bb25, %bb11
  %tmp14 = phi i64 [ 0, %bb11 ], [ %tmp27, %bb25 ]
  %tmp15 = phi i8* [ %tmp7, %bb11 ], [ %tmp30, %bb25 ]
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !13
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = trunc i64 %tmp16 to i32
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !13
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !12
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !13
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #10
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i32, i32* %arg1, i64 %tmp14
  store i32 %tmp17, i32* %tmp26, align 4, !tbaa !2
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !12
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #11
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !12
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #8
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_uint64_t_array(i8* %arg, i64* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #8
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 135, i8* getelementptr inbounds ([50 x i8], [50 x i8]* @__PRETTY_FUNCTION__.parse_uint64_t_array, i64 0, i64 0)) #9
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp8 = icmp ne i8* %tmp7, null
  %tmp9 = icmp sgt i32 %arg2, 0
  %tmp10 = and i1 %tmp9, %tmp8
  br i1 %tmp10, label %bb11, label %bb33

bb11:                                             ; preds = %bb6
  %tmp12 = sext i32 %arg2 to i64
  br label %bb13

bb13:                                             ; preds = %bb24, %bb11
  %tmp14 = phi i64 [ 0, %bb11 ], [ %tmp26, %bb24 ]
  %tmp15 = phi i8* [ %tmp7, %bb11 ], [ %tmp29, %bb24 ]
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !13
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !13
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !12
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !13
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #10
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds i64, i64* %arg1, i64 %tmp14
  store i64 %tmp16, i64* %tmp25, align 8, !tbaa !17
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !12
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #11
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !12
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #8
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_int8_t_array(i8* %arg, i8* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #8
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 136, i8* getelementptr inbounds ([46 x i8], [46 x i8]* @__PRETTY_FUNCTION__.parse_int8_t_array, i64 0, i64 0)) #9
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp8 = icmp ne i8* %tmp7, null
  %tmp9 = icmp sgt i32 %arg2, 0
  %tmp10 = and i1 %tmp9, %tmp8
  br i1 %tmp10, label %bb11, label %bb34

bb11:                                             ; preds = %bb6
  %tmp12 = sext i32 %arg2 to i64
  br label %bb13

bb13:                                             ; preds = %bb25, %bb11
  %tmp14 = phi i64 [ 0, %bb11 ], [ %tmp27, %bb25 ]
  %tmp15 = phi i8* [ %tmp7, %bb11 ], [ %tmp30, %bb25 ]
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !13
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = trunc i64 %tmp16 to i8
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !13
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !12
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !13
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #10
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i8, i8* %arg1, i64 %tmp14
  store i8 %tmp17, i8* %tmp26, align 1, !tbaa !12
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !12
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #11
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !12
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #8
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_int16_t_array(i8* %arg, i16* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #8
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 137, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_int16_t_array, i64 0, i64 0)) #9
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp8 = icmp ne i8* %tmp7, null
  %tmp9 = icmp sgt i32 %arg2, 0
  %tmp10 = and i1 %tmp9, %tmp8
  br i1 %tmp10, label %bb11, label %bb34

bb11:                                             ; preds = %bb6
  %tmp12 = sext i32 %arg2 to i64
  br label %bb13

bb13:                                             ; preds = %bb25, %bb11
  %tmp14 = phi i64 [ 0, %bb11 ], [ %tmp27, %bb25 ]
  %tmp15 = phi i8* [ %tmp7, %bb11 ], [ %tmp30, %bb25 ]
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !13
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = trunc i64 %tmp16 to i16
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !13
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !12
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !13
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #10
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i16, i16* %arg1, i64 %tmp14
  store i16 %tmp17, i16* %tmp26, align 2, !tbaa !15
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !12
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #11
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !12
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #8
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_int32_t_array(i8* %arg, i32* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #8
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 138, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_int32_t_array, i64 0, i64 0)) #9
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp8 = icmp ne i8* %tmp7, null
  %tmp9 = icmp sgt i32 %arg2, 0
  %tmp10 = and i1 %tmp9, %tmp8
  br i1 %tmp10, label %bb11, label %bb34

bb11:                                             ; preds = %bb6
  %tmp12 = sext i32 %arg2 to i64
  br label %bb13

bb13:                                             ; preds = %bb25, %bb11
  %tmp14 = phi i64 [ 0, %bb11 ], [ %tmp27, %bb25 ]
  %tmp15 = phi i8* [ %tmp7, %bb11 ], [ %tmp30, %bb25 ]
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !13
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = trunc i64 %tmp16 to i32
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !13
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !12
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !13
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #10
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i32, i32* %arg1, i64 %tmp14
  store i32 %tmp17, i32* %tmp26, align 4, !tbaa !2
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !12
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #11
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !12
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #8
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_int64_t_array(i8* %arg, i64* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #8
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 139, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_int64_t_array, i64 0, i64 0)) #9
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp8 = icmp ne i8* %tmp7, null
  %tmp9 = icmp sgt i32 %arg2, 0
  %tmp10 = and i1 %tmp9, %tmp8
  br i1 %tmp10, label %bb11, label %bb33

bb11:                                             ; preds = %bb6
  %tmp12 = sext i32 %arg2 to i64
  br label %bb13

bb13:                                             ; preds = %bb24, %bb11
  %tmp14 = phi i64 [ 0, %bb11 ], [ %tmp26, %bb24 ]
  %tmp15 = phi i8* [ %tmp7, %bb11 ], [ %tmp29, %bb24 ]
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !13
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !13
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !12
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !13
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #10
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds i64, i64* %arg1, i64 %tmp14
  store i64 %tmp16, i64* %tmp25, align 8, !tbaa !17
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !12
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #11
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !12
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #8
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_float_array(i8* %arg, float* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #8
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 141, i8* getelementptr inbounds ([44 x i8], [44 x i8]* @__PRETTY_FUNCTION__.parse_float_array, i64 0, i64 0)) #9
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp8 = icmp ne i8* %tmp7, null
  %tmp9 = icmp sgt i32 %arg2, 0
  %tmp10 = and i1 %tmp9, %tmp8
  br i1 %tmp10, label %bb11, label %bb33

bb11:                                             ; preds = %bb6
  %tmp12 = sext i32 %arg2 to i64
  br label %bb13

bb13:                                             ; preds = %bb24, %bb11
  %tmp14 = phi i64 [ 0, %bb11 ], [ %tmp26, %bb24 ]
  %tmp15 = phi i8* [ %tmp7, %bb11 ], [ %tmp29, %bb24 ]
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !13
  %tmp16 = call float @strtof(i8* nonnull %tmp15, i8** nonnull %tmp) #8
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !13
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !12
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !13
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #10
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds float, float* %arg1, i64 %tmp14
  store float %tmp16, float* %tmp25, align 4, !tbaa !18
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !12
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #11
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !12
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #8
  ret i32 0
}

; Function Attrs: nounwind
declare float @strtof(i8* readonly, i8** nocapture) local_unnamed_addr #2

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_double_array(i8* %arg, double* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #8
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 142, i8* getelementptr inbounds ([46 x i8], [46 x i8]* @__PRETTY_FUNCTION__.parse_double_array, i64 0, i64 0)) #9
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp8 = icmp ne i8* %tmp7, null
  %tmp9 = icmp sgt i32 %arg2, 0
  %tmp10 = and i1 %tmp9, %tmp8
  br i1 %tmp10, label %bb11, label %bb33

bb11:                                             ; preds = %bb6
  %tmp12 = sext i32 %arg2 to i64
  br label %bb13

bb13:                                             ; preds = %bb24, %bb11
  %tmp14 = phi i64 [ 0, %bb11 ], [ %tmp26, %bb24 ]
  %tmp15 = phi i8* [ %tmp7, %bb11 ], [ %tmp29, %bb24 ]
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !13
  %tmp16 = call double @strtod(i8* nonnull %tmp15, i8** nonnull %tmp) #8
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !13
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !12
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !13
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #10
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds double, double* %arg1, i64 %tmp14
  store double %tmp16, double* %tmp25, align 8, !tbaa !20
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !12
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #8
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #11
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !12
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #8
  ret i32 0
}

; Function Attrs: nounwind
declare double @strtod(i8* readonly, i8** nocapture) local_unnamed_addr #2

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_string(i32 %arg, i8* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 147, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @__PRETTY_FUNCTION__.write_string, i64 0, i64 0)) #9
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp slt i32 %arg2, 0
  br i1 %tmp5, label %bb6, label %bb9

bb6:                                              ; preds = %bb4
  %tmp7 = tail call i64 @strlen(i8* %arg1) #11
  %tmp8 = trunc i64 %tmp7 to i32
  br label %bb9

bb9:                                              ; preds = %bb6, %bb4
  %tmp10 = phi i32 [ %tmp8, %bb6 ], [ %arg2, %bb4 ]
  %tmp11 = icmp sgt i32 %tmp10, 0
  br i1 %tmp11, label %bb12, label %bb13

bb12:                                             ; preds = %bb9
  br label %bb16

bb13:                                             ; preds = %bb14, %bb9
  br label %bb27

bb14:                                             ; preds = %bb16
  %tmp15 = icmp sgt i32 %tmp10, %tmp25
  br i1 %tmp15, label %bb16, label %bb13

bb16:                                             ; preds = %bb14, %bb12
  %tmp17 = phi i32 [ %tmp25, %bb14 ], [ 0, %bb12 ]
  %tmp18 = sext i32 %tmp17 to i64
  %tmp19 = getelementptr inbounds i8, i8* %arg1, i64 %tmp18
  %tmp20 = sub nsw i32 %tmp10, %tmp17
  %tmp21 = sext i32 %tmp20 to i64
  %tmp22 = tail call i64 @write(i32 %arg, i8* %tmp19, i64 %tmp21) #8
  %tmp23 = trunc i64 %tmp22 to i32
  %tmp24 = icmp sgt i32 %tmp23, -1
  %tmp25 = add nsw i32 %tmp17, %tmp23
  br i1 %tmp24, label %bb14, label %bb26

bb26:                                             ; preds = %bb16
  tail call void @__assert_fail(i8* getelementptr inbounds ([28 x i8], [28 x i8]* @.str.16, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 154, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @__PRETTY_FUNCTION__.write_string, i64 0, i64 0)) #9
  unreachable

bb27:                                             ; preds = %bb32, %bb13
  %tmp28 = tail call i64 @write(i32 %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0), i64 1) #8
  %tmp29 = trunc i64 %tmp28 to i32
  %tmp30 = icmp sgt i32 %tmp29, -1
  br i1 %tmp30, label %bb32, label %bb31

bb31:                                             ; preds = %bb27
  tail call void @__assert_fail(i8* getelementptr inbounds ([28 x i8], [28 x i8]* @.str.16, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 160, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @__PRETTY_FUNCTION__.write_string, i64 0, i64 0)) #9
  unreachable

bb32:                                             ; preds = %bb27
  %tmp33 = icmp eq i32 %tmp29, 0
  br i1 %tmp33, label %bb27, label %bb34

bb34:                                             ; preds = %bb32
  ret i32 0
}

declare i64 @write(i32, i8* nocapture readonly, i64) local_unnamed_addr #6

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_uint8_t_array(i32 %arg, i8* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 177, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_uint8_t_array, i64 0, i64 0)) #9
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp sgt i32 %arg2, 0
  br i1 %tmp5, label %bb6, label %bb15

bb6:                                              ; preds = %bb4
  %tmp7 = zext i32 %arg2 to i64
  br label %bb8

bb8:                                              ; preds = %bb8, %bb6
  %tmp9 = phi i64 [ 0, %bb6 ], [ %tmp13, %bb8 ]
  %tmp10 = getelementptr inbounds i8, i8* %arg1, i64 %tmp9
  %tmp11 = load i8, i8* %tmp10, align 1, !tbaa !12
  %tmp12 = zext i8 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.17, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal void @fd_printf(i32 %arg, i8* nocapture readonly %arg1, ...) unnamed_addr #1 {
bb:
  %tmp = alloca [1 x %struct.__va_list_tag], align 16
  %tmp2 = alloca [256 x i8], align 16
  %tmp3 = bitcast [1 x %struct.__va_list_tag]* %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 24, i8* nonnull %tmp3) #8
  %tmp4 = getelementptr inbounds [256 x i8], [256 x i8]* %tmp2, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 256, i8* nonnull %tmp4) #8
  %tmp5 = getelementptr inbounds [1 x %struct.__va_list_tag], [1 x %struct.__va_list_tag]* %tmp, i64 0, i64 0
  call void @llvm.va_start(i8* nonnull %tmp3)
  %tmp6 = call i32 @vsnprintf(i8* nonnull %tmp4, i64 256, i8* %arg1, %struct.__va_list_tag* nonnull %tmp5) #8
  call void @llvm.va_end(i8* nonnull %tmp3)
  %tmp7 = icmp slt i32 %tmp6, 256
  br i1 %tmp7, label %bb9, label %bb8

bb8:                                              ; preds = %bb
  call void @__assert_fail(i8* getelementptr inbounds ([90 x i8], [90 x i8]* @.str.24, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 22, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.fd_printf, i64 0, i64 0)) #9
  unreachable

bb9:                                              ; preds = %bb
  %tmp10 = icmp sgt i32 %tmp6, 0
  br i1 %tmp10, label %bb11, label %bb25

bb11:                                             ; preds = %bb9
  br label %bb14

bb12:                                             ; preds = %bb14
  %tmp13 = icmp sgt i32 %tmp6, %tmp23
  br i1 %tmp13, label %bb14, label %bb25

bb14:                                             ; preds = %bb12, %bb11
  %tmp15 = phi i32 [ %tmp23, %bb12 ], [ 0, %bb11 ]
  %tmp16 = sext i32 %tmp15 to i64
  %tmp17 = getelementptr inbounds [256 x i8], [256 x i8]* %tmp2, i64 0, i64 %tmp16
  %tmp18 = sub nsw i32 %tmp6, %tmp15
  %tmp19 = sext i32 %tmp18 to i64
  %tmp20 = call i64 @write(i32 %arg, i8* nonnull %tmp17, i64 %tmp19) #8
  %tmp21 = trunc i64 %tmp20 to i32
  %tmp22 = icmp sgt i32 %tmp21, -1
  %tmp23 = add nsw i32 %tmp15, %tmp21
  br i1 %tmp22, label %bb12, label %bb24

bb24:                                             ; preds = %bb14
  call void @__assert_fail(i8* getelementptr inbounds ([28 x i8], [28 x i8]* @.str.16, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 26, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.fd_printf, i64 0, i64 0)) #9
  unreachable

bb25:                                             ; preds = %bb12, %bb9
  %tmp26 = phi i32 [ 0, %bb9 ], [ %tmp23, %bb12 ]
  %tmp27 = icmp eq i32 %tmp6, %tmp26
  br i1 %tmp27, label %bb29, label %bb28

bb28:                                             ; preds = %bb25
  call void @__assert_fail(i8* getelementptr inbounds ([50 x i8], [50 x i8]* @.str.26, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 29, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.fd_printf, i64 0, i64 0)) #9
  unreachable

bb29:                                             ; preds = %bb25
  call void @llvm.lifetime.end.p0i8(i64 256, i8* nonnull %tmp4) #8
  call void @llvm.lifetime.end.p0i8(i64 24, i8* nonnull %tmp3) #8
  ret void
}

; Function Attrs: nounwind
declare void @llvm.va_start(i8*) #8

; Function Attrs: nounwind
declare i32 @vsnprintf(i8* nocapture, i64, i8* nocapture readonly, %struct.__va_list_tag*) local_unnamed_addr #2

; Function Attrs: nounwind
declare void @llvm.va_end(i8*) #8

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_uint16_t_array(i32 %arg, i16* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 178, i8* getelementptr inbounds ([47 x i8], [47 x i8]* @__PRETTY_FUNCTION__.write_uint16_t_array, i64 0, i64 0)) #9
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp sgt i32 %arg2, 0
  br i1 %tmp5, label %bb6, label %bb15

bb6:                                              ; preds = %bb4
  %tmp7 = zext i32 %arg2 to i64
  br label %bb8

bb8:                                              ; preds = %bb8, %bb6
  %tmp9 = phi i64 [ 0, %bb6 ], [ %tmp13, %bb8 ]
  %tmp10 = getelementptr inbounds i16, i16* %arg1, i64 %tmp9
  %tmp11 = load i16, i16* %tmp10, align 2, !tbaa !15
  %tmp12 = zext i16 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.17, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_uint32_t_array(i32 %arg, i32* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 179, i8* getelementptr inbounds ([47 x i8], [47 x i8]* @__PRETTY_FUNCTION__.write_uint32_t_array, i64 0, i64 0)) #9
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp sgt i32 %arg2, 0
  br i1 %tmp5, label %bb6, label %bb14

bb6:                                              ; preds = %bb4
  %tmp7 = zext i32 %arg2 to i64
  br label %bb8

bb8:                                              ; preds = %bb8, %bb6
  %tmp9 = phi i64 [ 0, %bb6 ], [ %tmp12, %bb8 ]
  %tmp10 = getelementptr inbounds i32, i32* %arg1, i64 %tmp9
  %tmp11 = load i32, i32* %tmp10, align 4, !tbaa !2
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.17, i64 0, i64 0), i32 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_uint64_t_array(i32 %arg, i64* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 180, i8* getelementptr inbounds ([47 x i8], [47 x i8]* @__PRETTY_FUNCTION__.write_uint64_t_array, i64 0, i64 0)) #9
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp sgt i32 %arg2, 0
  br i1 %tmp5, label %bb6, label %bb14

bb6:                                              ; preds = %bb4
  %tmp7 = zext i32 %arg2 to i64
  br label %bb8

bb8:                                              ; preds = %bb8, %bb6
  %tmp9 = phi i64 [ 0, %bb6 ], [ %tmp12, %bb8 ]
  %tmp10 = getelementptr inbounds i64, i64* %arg1, i64 %tmp9
  %tmp11 = load i64, i64* %tmp10, align 8, !tbaa !17
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str.18, i64 0, i64 0), i64 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_int8_t_array(i32 %arg, i8* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 181, i8* getelementptr inbounds ([43 x i8], [43 x i8]* @__PRETTY_FUNCTION__.write_int8_t_array, i64 0, i64 0)) #9
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp sgt i32 %arg2, 0
  br i1 %tmp5, label %bb6, label %bb15

bb6:                                              ; preds = %bb4
  %tmp7 = zext i32 %arg2 to i64
  br label %bb8

bb8:                                              ; preds = %bb8, %bb6
  %tmp9 = phi i64 [ 0, %bb6 ], [ %tmp13, %bb8 ]
  %tmp10 = getelementptr inbounds i8, i8* %arg1, i64 %tmp9
  %tmp11 = load i8, i8* %tmp10, align 1, !tbaa !12
  %tmp12 = sext i8 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.19, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_int16_t_array(i32 %arg, i16* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 182, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_int16_t_array, i64 0, i64 0)) #9
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp sgt i32 %arg2, 0
  br i1 %tmp5, label %bb6, label %bb15

bb6:                                              ; preds = %bb4
  %tmp7 = zext i32 %arg2 to i64
  br label %bb8

bb8:                                              ; preds = %bb8, %bb6
  %tmp9 = phi i64 [ 0, %bb6 ], [ %tmp13, %bb8 ]
  %tmp10 = getelementptr inbounds i16, i16* %arg1, i64 %tmp9
  %tmp11 = load i16, i16* %tmp10, align 2, !tbaa !15
  %tmp12 = sext i16 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.19, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_int32_t_array(i32 %arg, i32* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 183, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_int32_t_array, i64 0, i64 0)) #9
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp sgt i32 %arg2, 0
  br i1 %tmp5, label %bb6, label %bb14

bb6:                                              ; preds = %bb4
  %tmp7 = zext i32 %arg2 to i64
  br label %bb8

bb8:                                              ; preds = %bb8, %bb6
  %tmp9 = phi i64 [ 0, %bb6 ], [ %tmp12, %bb8 ]
  %tmp10 = getelementptr inbounds i32, i32* %arg1, i64 %tmp9
  %tmp11 = load i32, i32* %tmp10, align 4, !tbaa !2
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.19, i64 0, i64 0), i32 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_int64_t_array(i32 %arg, i64* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 184, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_int64_t_array, i64 0, i64 0)) #9
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp sgt i32 %arg2, 0
  br i1 %tmp5, label %bb6, label %bb14

bb6:                                              ; preds = %bb4
  %tmp7 = zext i32 %arg2 to i64
  br label %bb8

bb8:                                              ; preds = %bb8, %bb6
  %tmp9 = phi i64 [ 0, %bb6 ], [ %tmp12, %bb8 ]
  %tmp10 = getelementptr inbounds i64, i64* %arg1, i64 %tmp9
  %tmp11 = load i64, i64* %tmp10, align 8, !tbaa !17
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str.20, i64 0, i64 0), i64 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_float_array(i32 %arg, float* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 186, i8* getelementptr inbounds ([41 x i8], [41 x i8]* @__PRETTY_FUNCTION__.write_float_array, i64 0, i64 0)) #9
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp sgt i32 %arg2, 0
  br i1 %tmp5, label %bb6, label %bb15

bb6:                                              ; preds = %bb4
  %tmp7 = zext i32 %arg2 to i64
  br label %bb8

bb8:                                              ; preds = %bb8, %bb6
  %tmp9 = phi i64 [ 0, %bb6 ], [ %tmp13, %bb8 ]
  %tmp10 = getelementptr inbounds float, float* %arg1, i64 %tmp9
  %tmp11 = load float, float* %tmp10, align 4, !tbaa !18
  %tmp12 = fpext float %tmp11 to double
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.21, i64 0, i64 0), double %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_double_array(i32 %arg, double* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 187, i8* getelementptr inbounds ([43 x i8], [43 x i8]* @__PRETTY_FUNCTION__.write_double_array, i64 0, i64 0)) #9
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp sgt i32 %arg2, 0
  br i1 %tmp5, label %bb6, label %bb14

bb6:                                              ; preds = %bb4
  %tmp7 = zext i32 %arg2 to i64
  br label %bb8

bb8:                                              ; preds = %bb8, %bb6
  %tmp9 = phi i64 [ 0, %bb6 ], [ %tmp12, %bb8 ]
  %tmp10 = getelementptr inbounds double, double* %arg1, i64 %tmp9
  %tmp11 = load double, double* %tmp10, align 8, !tbaa !20
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.21, i64 0, i64 0), double %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_section_header(i32 %arg) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb2, label %bb1

bb1:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 190, i8* getelementptr inbounds ([30 x i8], [30 x i8]* @__PRETTY_FUNCTION__.write_section_header, i64 0, i64 0)) #9
  unreachable

bb2:                                              ; preds = %bb
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.22, i64 0, i64 0))
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main(i32 %arg, i8** nocapture readonly %arg1) #1 {
bb:
  %tmp = icmp slt i32 %arg, 4
  br i1 %tmp, label %bb3, label %bb2

bb2:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([57 x i8], [57 x i8]* @.str.1.11, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.12, i64 0, i64 0), i32 21, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #9
  unreachable

bb3:                                              ; preds = %bb
  %tmp4 = icmp sgt i32 %arg, 1
  br i1 %tmp4, label %bb5, label %bb8

bb5:                                              ; preds = %bb3
  %tmp6 = getelementptr inbounds i8*, i8** %arg1, i64 1
  %tmp7 = load i8*, i8** %tmp6, align 8, !tbaa !13
  br label %bb8

bb8:                                              ; preds = %bb5, %bb3
  %tmp9 = phi i8* [ %tmp7, %bb5 ], [ getelementptr inbounds ([11 x i8], [11 x i8]* @.str.3, i64 0, i64 0), %bb3 ]
  %tmp10 = load i32, i32* @INPUT_SIZE, align 4, !tbaa !2
  %tmp11 = sext i32 %tmp10 to i64
  %tmp12 = tail call noalias i8* @malloc(i64 %tmp11) #8
  %tmp13 = icmp eq i8* %tmp12, null
  br i1 %tmp13, label %bb14, label %bb15

bb14:                                             ; preds = %bb8
  tail call void @__assert_fail(i8* getelementptr inbounds ([30 x i8], [30 x i8]* @.str.5, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.12, i64 0, i64 0), i32 37, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #9
  unreachable

bb15:                                             ; preds = %bb8
  %tmp16 = tail call i32 (i8*, i32, ...) @open(i8* %tmp9, i32 0) #8
  %tmp17 = icmp sgt i32 %tmp16, 0
  br i1 %tmp17, label %bb19, label %bb18

bb18:                                             ; preds = %bb15
  tail call void @__assert_fail(i8* getelementptr inbounds ([43 x i8], [43 x i8]* @.str.7, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.12, i64 0, i64 0), i32 39, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #9
  unreachable

bb19:                                             ; preds = %bb15
  tail call void @input_to_data(i32 %tmp16, i8* nonnull %tmp12) #8
  tail call void @run_benchmark(i8* nonnull %tmp12) #8
  tail call void @free(i8* nonnull %tmp12) #8
  %tmp20 = tail call i32 @puts(i8* getelementptr inbounds ([9 x i8], [9 x i8]* @str, i64 0, i64 0))
  ret i32 0
}

declare i32 @open(i8* nocapture readonly, i32, ...) local_unnamed_addr #6

; Function Attrs: nounwind
declare i32 @puts(i8* nocapture readonly) local_unnamed_addr #8

attributes #0 = { noinline norecurse nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { noinline norecurse nounwind readonly uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { argmemonly nounwind }
attributes #5 = { noreturn nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #6 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #7 = { argmemonly nounwind readonly "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #8 = { nounwind }
attributes #9 = { noreturn nounwind }
attributes #10 = { cold }
attributes #11 = { nounwind readonly }

!llvm.ident = !{!0, !0, !0, !0}
!llvm.module.flags = !{!1}

!0 = !{!"clang version 6.0.0 (git@github.com:seanzw/clang.git bb8d45f8ab88237f1fa0530b8ad9b96bf4a5e6cc) (git@github.com:seanzw/llvm.git 16ebb58ea40d384e8daa4c48d2bf7dd1ccfa5fcd)"}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{!3, !3, i64 0}
!3 = !{!"int", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C/C++ TBAA"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.isvectorized", i32 1}
!8 = !{!9, !10, i64 48}
!9 = !{!"stat", !10, i64 0, !10, i64 8, !10, i64 16, !3, i64 24, !3, i64 28, !3, i64 32, !3, i64 36, !10, i64 40, !10, i64 48, !10, i64 56, !10, i64 64, !11, i64 72, !11, i64 88, !11, i64 104, !4, i64 120}
!10 = !{!"long", !4, i64 0}
!11 = !{!"timespec", !10, i64 0, !10, i64 8}
!12 = !{!4, !4, i64 0}
!13 = !{!14, !14, i64 0}
!14 = !{!"any pointer", !4, i64 0}
!15 = !{!16, !16, i64 0}
!16 = !{!"short", !4, i64 0}
!17 = !{!10, !10, i64 0}
!18 = !{!19, !19, i64 0}
!19 = !{!"float", !4, i64 0}
!20 = !{!21, !21, i64 0}
!21 = !{!"double", !4, i64 0}
