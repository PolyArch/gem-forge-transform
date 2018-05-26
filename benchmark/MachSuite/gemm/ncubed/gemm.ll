; ModuleID = 'gemm.bc'
source_filename = "ld-temp.o"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct._IO_FILE = type { i32, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, %struct._IO_marker*, %struct._IO_FILE*, i32, i32, i64, i16, i8, [1 x i8], i8*, i64, i8*, i8*, i8*, i8*, i64, i32, [20 x i8] }
%struct._IO_marker = type { %struct._IO_marker*, %struct._IO_FILE*, i32 }
%struct.stat = type { i64, i64, i64, i32, i32, i32, i32, i64, i64, i64, i64, %struct.timespec, %struct.timespec, %struct.timespec, [3 x i64] }
%struct.timespec = type { i64, i64 }
%struct.__va_list_tag = type { i32, i32, i8*, i8* }

@INPUT_SIZE = internal dso_local global i32 24576, align 4
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

; Function Attrs: norecurse nounwind uwtable
define internal dso_local void @gemm(double* nocapture readonly %arg, double* nocapture readonly %arg1, double* nocapture %arg2) #0 {
bb:
  %tmp = getelementptr double, double* %arg1, i64 1024
  br label %bb4

bb3:                                              ; preds = %bb584
  ret void

bb4:                                              ; preds = %bb584, %bb
  %tmp5 = phi i32 [ 0, %bb ], [ %tmp585, %bb584 ]
  br label %bb6

bb6:                                              ; preds = %bb581, %bb4
  %tmp7 = phi i64 [ 0, %bb4 ], [ %tmp582, %bb581 ]
  %tmp8 = shl i64 %tmp7, 5
  %tmp9 = getelementptr double, double* %arg2, i64 %tmp8
  %tmp10 = add i64 %tmp8, 32
  %tmp11 = getelementptr double, double* %arg2, i64 %tmp10
  %tmp12 = shl nsw i64 %tmp7, 5
  %tmp13 = getelementptr inbounds double, double* %arg, i64 %tmp12
  %tmp14 = or i64 %tmp12, 1
  %tmp15 = getelementptr inbounds double, double* %arg, i64 %tmp14
  %tmp16 = or i64 %tmp12, 2
  %tmp17 = getelementptr inbounds double, double* %arg, i64 %tmp16
  %tmp18 = or i64 %tmp12, 3
  %tmp19 = getelementptr inbounds double, double* %arg, i64 %tmp18
  %tmp20 = or i64 %tmp12, 4
  %tmp21 = getelementptr inbounds double, double* %arg, i64 %tmp20
  %tmp22 = or i64 %tmp12, 5
  %tmp23 = getelementptr inbounds double, double* %arg, i64 %tmp22
  %tmp24 = or i64 %tmp12, 6
  %tmp25 = getelementptr inbounds double, double* %arg, i64 %tmp24
  %tmp26 = or i64 %tmp12, 7
  %tmp27 = getelementptr inbounds double, double* %arg, i64 %tmp26
  %tmp28 = or i64 %tmp12, 8
  %tmp29 = getelementptr inbounds double, double* %arg, i64 %tmp28
  %tmp30 = or i64 %tmp12, 9
  %tmp31 = getelementptr inbounds double, double* %arg, i64 %tmp30
  %tmp32 = or i64 %tmp12, 10
  %tmp33 = getelementptr inbounds double, double* %arg, i64 %tmp32
  %tmp34 = or i64 %tmp12, 11
  %tmp35 = getelementptr inbounds double, double* %arg, i64 %tmp34
  %tmp36 = or i64 %tmp12, 12
  %tmp37 = getelementptr inbounds double, double* %arg, i64 %tmp36
  %tmp38 = or i64 %tmp12, 13
  %tmp39 = getelementptr inbounds double, double* %arg, i64 %tmp38
  %tmp40 = or i64 %tmp12, 14
  %tmp41 = getelementptr inbounds double, double* %arg, i64 %tmp40
  %tmp42 = or i64 %tmp12, 15
  %tmp43 = getelementptr inbounds double, double* %arg, i64 %tmp42
  %tmp44 = or i64 %tmp12, 16
  %tmp45 = getelementptr inbounds double, double* %arg, i64 %tmp44
  %tmp46 = or i64 %tmp12, 17
  %tmp47 = getelementptr inbounds double, double* %arg, i64 %tmp46
  %tmp48 = or i64 %tmp12, 18
  %tmp49 = getelementptr inbounds double, double* %arg, i64 %tmp48
  %tmp50 = or i64 %tmp12, 19
  %tmp51 = getelementptr inbounds double, double* %arg, i64 %tmp50
  %tmp52 = or i64 %tmp12, 20
  %tmp53 = getelementptr inbounds double, double* %arg, i64 %tmp52
  %tmp54 = or i64 %tmp12, 21
  %tmp55 = getelementptr inbounds double, double* %arg, i64 %tmp54
  %tmp56 = or i64 %tmp12, 22
  %tmp57 = getelementptr inbounds double, double* %arg, i64 %tmp56
  %tmp58 = or i64 %tmp12, 23
  %tmp59 = getelementptr inbounds double, double* %arg, i64 %tmp58
  %tmp60 = or i64 %tmp12, 24
  %tmp61 = getelementptr inbounds double, double* %arg, i64 %tmp60
  %tmp62 = or i64 %tmp12, 25
  %tmp63 = getelementptr inbounds double, double* %arg, i64 %tmp62
  %tmp64 = or i64 %tmp12, 26
  %tmp65 = getelementptr inbounds double, double* %arg, i64 %tmp64
  %tmp66 = or i64 %tmp12, 27
  %tmp67 = getelementptr inbounds double, double* %arg, i64 %tmp66
  %tmp68 = or i64 %tmp12, 28
  %tmp69 = getelementptr inbounds double, double* %arg, i64 %tmp68
  %tmp70 = or i64 %tmp12, 29
  %tmp71 = getelementptr inbounds double, double* %arg, i64 %tmp70
  %tmp72 = or i64 %tmp12, 30
  %tmp73 = getelementptr inbounds double, double* %arg, i64 %tmp72
  %tmp74 = or i64 %tmp12, 31
  %tmp75 = getelementptr inbounds double, double* %arg, i64 %tmp74
  %tmp76 = bitcast double* %tmp9 to i8*
  %tmp77 = or i64 %tmp8, 31
  %tmp78 = getelementptr double, double* %arg, i64 %tmp77
  %tmp79 = bitcast double* %tmp78 to i8*
  %tmp80 = getelementptr i8, i8* %tmp79, i64 1
  %tmp81 = icmp ugt i8* %tmp80, %tmp76
  %tmp82 = icmp ult double* %tmp75, %tmp11
  %tmp83 = and i1 %tmp81, %tmp82
  %tmp84 = icmp ult double* %tmp9, %tmp
  %tmp85 = icmp ugt double* %tmp11, %arg1
  %tmp86 = and i1 %tmp84, %tmp85
  %tmp87 = or i1 %tmp83, %tmp86
  br i1 %tmp87, label %bb185, label %bb88

bb88:                                             ; preds = %bb6
  %tmp89 = load double, double* %tmp13, align 8, !tbaa !2, !alias.scope !6
  %tmp90 = insertelement <2 x double> undef, double %tmp89, i32 0
  %tmp91 = shufflevector <2 x double> %tmp90, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp92 = load double, double* %tmp15, align 8, !tbaa !2, !alias.scope !6
  %tmp93 = insertelement <2 x double> undef, double %tmp92, i32 0
  %tmp94 = shufflevector <2 x double> %tmp93, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp95 = load double, double* %tmp17, align 8, !tbaa !2, !alias.scope !6
  %tmp96 = insertelement <2 x double> undef, double %tmp95, i32 0
  %tmp97 = shufflevector <2 x double> %tmp96, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp98 = load double, double* %tmp19, align 8, !tbaa !2, !alias.scope !6
  %tmp99 = insertelement <2 x double> undef, double %tmp98, i32 0
  %tmp100 = shufflevector <2 x double> %tmp99, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp101 = load double, double* %tmp21, align 8, !tbaa !2, !alias.scope !6
  %tmp102 = insertelement <2 x double> undef, double %tmp101, i32 0
  %tmp103 = shufflevector <2 x double> %tmp102, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp104 = load double, double* %tmp23, align 8, !tbaa !2, !alias.scope !6
  %tmp105 = insertelement <2 x double> undef, double %tmp104, i32 0
  %tmp106 = shufflevector <2 x double> %tmp105, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp107 = load double, double* %tmp25, align 8, !tbaa !2, !alias.scope !6
  %tmp108 = insertelement <2 x double> undef, double %tmp107, i32 0
  %tmp109 = shufflevector <2 x double> %tmp108, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp110 = load double, double* %tmp27, align 8, !tbaa !2, !alias.scope !6
  %tmp111 = insertelement <2 x double> undef, double %tmp110, i32 0
  %tmp112 = shufflevector <2 x double> %tmp111, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp113 = load double, double* %tmp29, align 8, !tbaa !2, !alias.scope !6
  %tmp114 = insertelement <2 x double> undef, double %tmp113, i32 0
  %tmp115 = shufflevector <2 x double> %tmp114, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp116 = load double, double* %tmp31, align 8, !tbaa !2, !alias.scope !6
  %tmp117 = insertelement <2 x double> undef, double %tmp116, i32 0
  %tmp118 = shufflevector <2 x double> %tmp117, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp119 = load double, double* %tmp33, align 8, !tbaa !2, !alias.scope !6
  %tmp120 = insertelement <2 x double> undef, double %tmp119, i32 0
  %tmp121 = shufflevector <2 x double> %tmp120, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp122 = load double, double* %tmp35, align 8, !tbaa !2, !alias.scope !6
  %tmp123 = insertelement <2 x double> undef, double %tmp122, i32 0
  %tmp124 = shufflevector <2 x double> %tmp123, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp125 = load double, double* %tmp37, align 8, !tbaa !2, !alias.scope !6
  %tmp126 = insertelement <2 x double> undef, double %tmp125, i32 0
  %tmp127 = shufflevector <2 x double> %tmp126, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp128 = load double, double* %tmp39, align 8, !tbaa !2, !alias.scope !6
  %tmp129 = insertelement <2 x double> undef, double %tmp128, i32 0
  %tmp130 = shufflevector <2 x double> %tmp129, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp131 = load double, double* %tmp41, align 8, !tbaa !2, !alias.scope !6
  %tmp132 = insertelement <2 x double> undef, double %tmp131, i32 0
  %tmp133 = shufflevector <2 x double> %tmp132, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp134 = load double, double* %tmp43, align 8, !tbaa !2, !alias.scope !6
  %tmp135 = insertelement <2 x double> undef, double %tmp134, i32 0
  %tmp136 = shufflevector <2 x double> %tmp135, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp137 = load double, double* %tmp45, align 8, !tbaa !2, !alias.scope !6
  %tmp138 = insertelement <2 x double> undef, double %tmp137, i32 0
  %tmp139 = shufflevector <2 x double> %tmp138, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp140 = load double, double* %tmp47, align 8, !tbaa !2, !alias.scope !6
  %tmp141 = insertelement <2 x double> undef, double %tmp140, i32 0
  %tmp142 = shufflevector <2 x double> %tmp141, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp143 = load double, double* %tmp49, align 8, !tbaa !2, !alias.scope !6
  %tmp144 = insertelement <2 x double> undef, double %tmp143, i32 0
  %tmp145 = shufflevector <2 x double> %tmp144, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp146 = load double, double* %tmp51, align 8, !tbaa !2, !alias.scope !6
  %tmp147 = insertelement <2 x double> undef, double %tmp146, i32 0
  %tmp148 = shufflevector <2 x double> %tmp147, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp149 = load double, double* %tmp53, align 8, !tbaa !2, !alias.scope !6
  %tmp150 = insertelement <2 x double> undef, double %tmp149, i32 0
  %tmp151 = shufflevector <2 x double> %tmp150, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp152 = load double, double* %tmp55, align 8, !tbaa !2, !alias.scope !6
  %tmp153 = insertelement <2 x double> undef, double %tmp152, i32 0
  %tmp154 = shufflevector <2 x double> %tmp153, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp155 = load double, double* %tmp57, align 8, !tbaa !2, !alias.scope !6
  %tmp156 = insertelement <2 x double> undef, double %tmp155, i32 0
  %tmp157 = shufflevector <2 x double> %tmp156, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp158 = load double, double* %tmp59, align 8, !tbaa !2, !alias.scope !6
  %tmp159 = insertelement <2 x double> undef, double %tmp158, i32 0
  %tmp160 = shufflevector <2 x double> %tmp159, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp161 = load double, double* %tmp61, align 8, !tbaa !2, !alias.scope !6
  %tmp162 = insertelement <2 x double> undef, double %tmp161, i32 0
  %tmp163 = shufflevector <2 x double> %tmp162, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp164 = load double, double* %tmp63, align 8, !tbaa !2, !alias.scope !6
  %tmp165 = insertelement <2 x double> undef, double %tmp164, i32 0
  %tmp166 = shufflevector <2 x double> %tmp165, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp167 = load double, double* %tmp65, align 8, !tbaa !2, !alias.scope !6
  %tmp168 = insertelement <2 x double> undef, double %tmp167, i32 0
  %tmp169 = shufflevector <2 x double> %tmp168, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp170 = load double, double* %tmp67, align 8, !tbaa !2, !alias.scope !6
  %tmp171 = insertelement <2 x double> undef, double %tmp170, i32 0
  %tmp172 = shufflevector <2 x double> %tmp171, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp173 = load double, double* %tmp69, align 8, !tbaa !2, !alias.scope !6
  %tmp174 = insertelement <2 x double> undef, double %tmp173, i32 0
  %tmp175 = shufflevector <2 x double> %tmp174, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp176 = load double, double* %tmp71, align 8, !tbaa !2, !alias.scope !6
  %tmp177 = insertelement <2 x double> undef, double %tmp176, i32 0
  %tmp178 = shufflevector <2 x double> %tmp177, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp179 = load double, double* %tmp73, align 8, !tbaa !2, !alias.scope !6
  %tmp180 = insertelement <2 x double> undef, double %tmp179, i32 0
  %tmp181 = shufflevector <2 x double> %tmp180, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp182 = load double, double* %tmp75, align 8, !tbaa !2, !alias.scope !6
  %tmp183 = insertelement <2 x double> undef, double %tmp182, i32 0
  %tmp184 = shufflevector <2 x double> %tmp183, <2 x double> undef, <2 x i32> zeroinitializer
  br label %bb186

bb185:                                            ; preds = %bb6
  br label %bb384

bb186:                                            ; preds = %bb186, %bb88
  %tmp187 = phi i64 [ %tmp382, %bb186 ], [ 0, %bb88 ]
  %tmp188 = getelementptr inbounds double, double* %arg1, i64 %tmp187
  %tmp189 = bitcast double* %tmp188 to <2 x double>*
  %tmp190 = load <2 x double>, <2 x double>* %tmp189, align 8, !tbaa !2, !alias.scope !9
  %tmp191 = fmul <2 x double> %tmp91, %tmp190
  %tmp192 = fadd <2 x double> %tmp191, zeroinitializer
  %tmp193 = add nuw nsw i64 %tmp187, 32
  %tmp194 = getelementptr inbounds double, double* %arg1, i64 %tmp193
  %tmp195 = bitcast double* %tmp194 to <2 x double>*
  %tmp196 = load <2 x double>, <2 x double>* %tmp195, align 8, !tbaa !2, !alias.scope !9
  %tmp197 = fmul <2 x double> %tmp94, %tmp196
  %tmp198 = fadd <2 x double> %tmp192, %tmp197
  %tmp199 = add nuw nsw i64 %tmp187, 64
  %tmp200 = getelementptr inbounds double, double* %arg1, i64 %tmp199
  %tmp201 = bitcast double* %tmp200 to <2 x double>*
  %tmp202 = load <2 x double>, <2 x double>* %tmp201, align 8, !tbaa !2, !alias.scope !9
  %tmp203 = fmul <2 x double> %tmp97, %tmp202
  %tmp204 = fadd <2 x double> %tmp198, %tmp203
  %tmp205 = add nuw nsw i64 %tmp187, 96
  %tmp206 = getelementptr inbounds double, double* %arg1, i64 %tmp205
  %tmp207 = bitcast double* %tmp206 to <2 x double>*
  %tmp208 = load <2 x double>, <2 x double>* %tmp207, align 8, !tbaa !2, !alias.scope !9
  %tmp209 = fmul <2 x double> %tmp100, %tmp208
  %tmp210 = fadd <2 x double> %tmp204, %tmp209
  %tmp211 = add nuw nsw i64 %tmp187, 128
  %tmp212 = getelementptr inbounds double, double* %arg1, i64 %tmp211
  %tmp213 = bitcast double* %tmp212 to <2 x double>*
  %tmp214 = load <2 x double>, <2 x double>* %tmp213, align 8, !tbaa !2, !alias.scope !9
  %tmp215 = fmul <2 x double> %tmp103, %tmp214
  %tmp216 = fadd <2 x double> %tmp210, %tmp215
  %tmp217 = add nuw nsw i64 %tmp187, 160
  %tmp218 = getelementptr inbounds double, double* %arg1, i64 %tmp217
  %tmp219 = bitcast double* %tmp218 to <2 x double>*
  %tmp220 = load <2 x double>, <2 x double>* %tmp219, align 8, !tbaa !2, !alias.scope !9
  %tmp221 = fmul <2 x double> %tmp106, %tmp220
  %tmp222 = fadd <2 x double> %tmp216, %tmp221
  %tmp223 = add nuw nsw i64 %tmp187, 192
  %tmp224 = getelementptr inbounds double, double* %arg1, i64 %tmp223
  %tmp225 = bitcast double* %tmp224 to <2 x double>*
  %tmp226 = load <2 x double>, <2 x double>* %tmp225, align 8, !tbaa !2, !alias.scope !9
  %tmp227 = fmul <2 x double> %tmp109, %tmp226
  %tmp228 = fadd <2 x double> %tmp222, %tmp227
  %tmp229 = add nuw nsw i64 %tmp187, 224
  %tmp230 = getelementptr inbounds double, double* %arg1, i64 %tmp229
  %tmp231 = bitcast double* %tmp230 to <2 x double>*
  %tmp232 = load <2 x double>, <2 x double>* %tmp231, align 8, !tbaa !2, !alias.scope !9
  %tmp233 = fmul <2 x double> %tmp112, %tmp232
  %tmp234 = fadd <2 x double> %tmp228, %tmp233
  %tmp235 = add nuw nsw i64 %tmp187, 256
  %tmp236 = getelementptr inbounds double, double* %arg1, i64 %tmp235
  %tmp237 = bitcast double* %tmp236 to <2 x double>*
  %tmp238 = load <2 x double>, <2 x double>* %tmp237, align 8, !tbaa !2, !alias.scope !9
  %tmp239 = fmul <2 x double> %tmp115, %tmp238
  %tmp240 = fadd <2 x double> %tmp234, %tmp239
  %tmp241 = add nuw nsw i64 %tmp187, 288
  %tmp242 = getelementptr inbounds double, double* %arg1, i64 %tmp241
  %tmp243 = bitcast double* %tmp242 to <2 x double>*
  %tmp244 = load <2 x double>, <2 x double>* %tmp243, align 8, !tbaa !2, !alias.scope !9
  %tmp245 = fmul <2 x double> %tmp118, %tmp244
  %tmp246 = fadd <2 x double> %tmp240, %tmp245
  %tmp247 = add nuw nsw i64 %tmp187, 320
  %tmp248 = getelementptr inbounds double, double* %arg1, i64 %tmp247
  %tmp249 = bitcast double* %tmp248 to <2 x double>*
  %tmp250 = load <2 x double>, <2 x double>* %tmp249, align 8, !tbaa !2, !alias.scope !9
  %tmp251 = fmul <2 x double> %tmp121, %tmp250
  %tmp252 = fadd <2 x double> %tmp246, %tmp251
  %tmp253 = add nuw nsw i64 %tmp187, 352
  %tmp254 = getelementptr inbounds double, double* %arg1, i64 %tmp253
  %tmp255 = bitcast double* %tmp254 to <2 x double>*
  %tmp256 = load <2 x double>, <2 x double>* %tmp255, align 8, !tbaa !2, !alias.scope !9
  %tmp257 = fmul <2 x double> %tmp124, %tmp256
  %tmp258 = fadd <2 x double> %tmp252, %tmp257
  %tmp259 = add nuw nsw i64 %tmp187, 384
  %tmp260 = getelementptr inbounds double, double* %arg1, i64 %tmp259
  %tmp261 = bitcast double* %tmp260 to <2 x double>*
  %tmp262 = load <2 x double>, <2 x double>* %tmp261, align 8, !tbaa !2, !alias.scope !9
  %tmp263 = fmul <2 x double> %tmp127, %tmp262
  %tmp264 = fadd <2 x double> %tmp258, %tmp263
  %tmp265 = add nuw nsw i64 %tmp187, 416
  %tmp266 = getelementptr inbounds double, double* %arg1, i64 %tmp265
  %tmp267 = bitcast double* %tmp266 to <2 x double>*
  %tmp268 = load <2 x double>, <2 x double>* %tmp267, align 8, !tbaa !2, !alias.scope !9
  %tmp269 = fmul <2 x double> %tmp130, %tmp268
  %tmp270 = fadd <2 x double> %tmp264, %tmp269
  %tmp271 = add nuw nsw i64 %tmp187, 448
  %tmp272 = getelementptr inbounds double, double* %arg1, i64 %tmp271
  %tmp273 = bitcast double* %tmp272 to <2 x double>*
  %tmp274 = load <2 x double>, <2 x double>* %tmp273, align 8, !tbaa !2, !alias.scope !9
  %tmp275 = fmul <2 x double> %tmp133, %tmp274
  %tmp276 = fadd <2 x double> %tmp270, %tmp275
  %tmp277 = add nuw nsw i64 %tmp187, 480
  %tmp278 = getelementptr inbounds double, double* %arg1, i64 %tmp277
  %tmp279 = bitcast double* %tmp278 to <2 x double>*
  %tmp280 = load <2 x double>, <2 x double>* %tmp279, align 8, !tbaa !2, !alias.scope !9
  %tmp281 = fmul <2 x double> %tmp136, %tmp280
  %tmp282 = fadd <2 x double> %tmp276, %tmp281
  %tmp283 = add nuw nsw i64 %tmp187, 512
  %tmp284 = getelementptr inbounds double, double* %arg1, i64 %tmp283
  %tmp285 = bitcast double* %tmp284 to <2 x double>*
  %tmp286 = load <2 x double>, <2 x double>* %tmp285, align 8, !tbaa !2, !alias.scope !9
  %tmp287 = fmul <2 x double> %tmp139, %tmp286
  %tmp288 = fadd <2 x double> %tmp282, %tmp287
  %tmp289 = add nuw nsw i64 %tmp187, 544
  %tmp290 = getelementptr inbounds double, double* %arg1, i64 %tmp289
  %tmp291 = bitcast double* %tmp290 to <2 x double>*
  %tmp292 = load <2 x double>, <2 x double>* %tmp291, align 8, !tbaa !2, !alias.scope !9
  %tmp293 = fmul <2 x double> %tmp142, %tmp292
  %tmp294 = fadd <2 x double> %tmp288, %tmp293
  %tmp295 = add nuw nsw i64 %tmp187, 576
  %tmp296 = getelementptr inbounds double, double* %arg1, i64 %tmp295
  %tmp297 = bitcast double* %tmp296 to <2 x double>*
  %tmp298 = load <2 x double>, <2 x double>* %tmp297, align 8, !tbaa !2, !alias.scope !9
  %tmp299 = fmul <2 x double> %tmp145, %tmp298
  %tmp300 = fadd <2 x double> %tmp294, %tmp299
  %tmp301 = add nuw nsw i64 %tmp187, 608
  %tmp302 = getelementptr inbounds double, double* %arg1, i64 %tmp301
  %tmp303 = bitcast double* %tmp302 to <2 x double>*
  %tmp304 = load <2 x double>, <2 x double>* %tmp303, align 8, !tbaa !2, !alias.scope !9
  %tmp305 = fmul <2 x double> %tmp148, %tmp304
  %tmp306 = fadd <2 x double> %tmp300, %tmp305
  %tmp307 = add nuw nsw i64 %tmp187, 640
  %tmp308 = getelementptr inbounds double, double* %arg1, i64 %tmp307
  %tmp309 = bitcast double* %tmp308 to <2 x double>*
  %tmp310 = load <2 x double>, <2 x double>* %tmp309, align 8, !tbaa !2, !alias.scope !9
  %tmp311 = fmul <2 x double> %tmp151, %tmp310
  %tmp312 = fadd <2 x double> %tmp306, %tmp311
  %tmp313 = add nuw nsw i64 %tmp187, 672
  %tmp314 = getelementptr inbounds double, double* %arg1, i64 %tmp313
  %tmp315 = bitcast double* %tmp314 to <2 x double>*
  %tmp316 = load <2 x double>, <2 x double>* %tmp315, align 8, !tbaa !2, !alias.scope !9
  %tmp317 = fmul <2 x double> %tmp154, %tmp316
  %tmp318 = fadd <2 x double> %tmp312, %tmp317
  %tmp319 = add nuw nsw i64 %tmp187, 704
  %tmp320 = getelementptr inbounds double, double* %arg1, i64 %tmp319
  %tmp321 = bitcast double* %tmp320 to <2 x double>*
  %tmp322 = load <2 x double>, <2 x double>* %tmp321, align 8, !tbaa !2, !alias.scope !9
  %tmp323 = fmul <2 x double> %tmp157, %tmp322
  %tmp324 = fadd <2 x double> %tmp318, %tmp323
  %tmp325 = add nuw nsw i64 %tmp187, 736
  %tmp326 = getelementptr inbounds double, double* %arg1, i64 %tmp325
  %tmp327 = bitcast double* %tmp326 to <2 x double>*
  %tmp328 = load <2 x double>, <2 x double>* %tmp327, align 8, !tbaa !2, !alias.scope !9
  %tmp329 = fmul <2 x double> %tmp160, %tmp328
  %tmp330 = fadd <2 x double> %tmp324, %tmp329
  %tmp331 = add nuw nsw i64 %tmp187, 768
  %tmp332 = getelementptr inbounds double, double* %arg1, i64 %tmp331
  %tmp333 = bitcast double* %tmp332 to <2 x double>*
  %tmp334 = load <2 x double>, <2 x double>* %tmp333, align 8, !tbaa !2, !alias.scope !9
  %tmp335 = fmul <2 x double> %tmp163, %tmp334
  %tmp336 = fadd <2 x double> %tmp330, %tmp335
  %tmp337 = add nuw nsw i64 %tmp187, 800
  %tmp338 = getelementptr inbounds double, double* %arg1, i64 %tmp337
  %tmp339 = bitcast double* %tmp338 to <2 x double>*
  %tmp340 = load <2 x double>, <2 x double>* %tmp339, align 8, !tbaa !2, !alias.scope !9
  %tmp341 = fmul <2 x double> %tmp166, %tmp340
  %tmp342 = fadd <2 x double> %tmp336, %tmp341
  %tmp343 = add nuw nsw i64 %tmp187, 832
  %tmp344 = getelementptr inbounds double, double* %arg1, i64 %tmp343
  %tmp345 = bitcast double* %tmp344 to <2 x double>*
  %tmp346 = load <2 x double>, <2 x double>* %tmp345, align 8, !tbaa !2, !alias.scope !9
  %tmp347 = fmul <2 x double> %tmp169, %tmp346
  %tmp348 = fadd <2 x double> %tmp342, %tmp347
  %tmp349 = add nuw nsw i64 %tmp187, 864
  %tmp350 = getelementptr inbounds double, double* %arg1, i64 %tmp349
  %tmp351 = bitcast double* %tmp350 to <2 x double>*
  %tmp352 = load <2 x double>, <2 x double>* %tmp351, align 8, !tbaa !2, !alias.scope !9
  %tmp353 = fmul <2 x double> %tmp172, %tmp352
  %tmp354 = fadd <2 x double> %tmp348, %tmp353
  %tmp355 = add nuw nsw i64 %tmp187, 896
  %tmp356 = getelementptr inbounds double, double* %arg1, i64 %tmp355
  %tmp357 = bitcast double* %tmp356 to <2 x double>*
  %tmp358 = load <2 x double>, <2 x double>* %tmp357, align 8, !tbaa !2, !alias.scope !9
  %tmp359 = fmul <2 x double> %tmp175, %tmp358
  %tmp360 = fadd <2 x double> %tmp354, %tmp359
  %tmp361 = add nuw nsw i64 %tmp187, 928
  %tmp362 = getelementptr inbounds double, double* %arg1, i64 %tmp361
  %tmp363 = bitcast double* %tmp362 to <2 x double>*
  %tmp364 = load <2 x double>, <2 x double>* %tmp363, align 8, !tbaa !2, !alias.scope !9
  %tmp365 = fmul <2 x double> %tmp178, %tmp364
  %tmp366 = fadd <2 x double> %tmp360, %tmp365
  %tmp367 = add nuw nsw i64 %tmp187, 960
  %tmp368 = getelementptr inbounds double, double* %arg1, i64 %tmp367
  %tmp369 = bitcast double* %tmp368 to <2 x double>*
  %tmp370 = load <2 x double>, <2 x double>* %tmp369, align 8, !tbaa !2, !alias.scope !9
  %tmp371 = fmul <2 x double> %tmp181, %tmp370
  %tmp372 = fadd <2 x double> %tmp366, %tmp371
  %tmp373 = add nuw nsw i64 %tmp187, 992
  %tmp374 = getelementptr inbounds double, double* %arg1, i64 %tmp373
  %tmp375 = bitcast double* %tmp374 to <2 x double>*
  %tmp376 = load <2 x double>, <2 x double>* %tmp375, align 8, !tbaa !2, !alias.scope !9
  %tmp377 = fmul <2 x double> %tmp184, %tmp376
  %tmp378 = fadd <2 x double> %tmp372, %tmp377
  %tmp379 = add nuw nsw i64 %tmp187, %tmp12
  %tmp380 = getelementptr inbounds double, double* %arg2, i64 %tmp379
  %tmp381 = bitcast double* %tmp380 to <2 x double>*
  store <2 x double> %tmp378, <2 x double>* %tmp381, align 8, !tbaa !2, !alias.scope !11, !noalias !13
  %tmp382 = add i64 %tmp187, 2
  %tmp383 = icmp eq i64 %tmp382, 32
  br i1 %tmp383, label %bb581, label %bb186, !llvm.loop !14

bb384:                                            ; preds = %bb384, %bb185
  %tmp385 = phi i64 [ %tmp579, %bb384 ], [ 0, %bb185 ]
  %tmp386 = load double, double* %tmp13, align 8, !tbaa !2
  %tmp387 = getelementptr inbounds double, double* %arg1, i64 %tmp385
  %tmp388 = load double, double* %tmp387, align 8, !tbaa !2
  %tmp389 = fmul double %tmp386, %tmp388
  %tmp390 = fadd double %tmp389, 0.000000e+00
  %tmp391 = load double, double* %tmp15, align 8, !tbaa !2
  %tmp392 = add nuw nsw i64 %tmp385, 32
  %tmp393 = getelementptr inbounds double, double* %arg1, i64 %tmp392
  %tmp394 = load double, double* %tmp393, align 8, !tbaa !2
  %tmp395 = fmul double %tmp391, %tmp394
  %tmp396 = fadd double %tmp390, %tmp395
  %tmp397 = load double, double* %tmp17, align 8, !tbaa !2
  %tmp398 = add nuw nsw i64 %tmp385, 64
  %tmp399 = getelementptr inbounds double, double* %arg1, i64 %tmp398
  %tmp400 = load double, double* %tmp399, align 8, !tbaa !2
  %tmp401 = fmul double %tmp397, %tmp400
  %tmp402 = fadd double %tmp396, %tmp401
  %tmp403 = load double, double* %tmp19, align 8, !tbaa !2
  %tmp404 = add nuw nsw i64 %tmp385, 96
  %tmp405 = getelementptr inbounds double, double* %arg1, i64 %tmp404
  %tmp406 = load double, double* %tmp405, align 8, !tbaa !2
  %tmp407 = fmul double %tmp403, %tmp406
  %tmp408 = fadd double %tmp402, %tmp407
  %tmp409 = load double, double* %tmp21, align 8, !tbaa !2
  %tmp410 = add nuw nsw i64 %tmp385, 128
  %tmp411 = getelementptr inbounds double, double* %arg1, i64 %tmp410
  %tmp412 = load double, double* %tmp411, align 8, !tbaa !2
  %tmp413 = fmul double %tmp409, %tmp412
  %tmp414 = fadd double %tmp408, %tmp413
  %tmp415 = load double, double* %tmp23, align 8, !tbaa !2
  %tmp416 = add nuw nsw i64 %tmp385, 160
  %tmp417 = getelementptr inbounds double, double* %arg1, i64 %tmp416
  %tmp418 = load double, double* %tmp417, align 8, !tbaa !2
  %tmp419 = fmul double %tmp415, %tmp418
  %tmp420 = fadd double %tmp414, %tmp419
  %tmp421 = load double, double* %tmp25, align 8, !tbaa !2
  %tmp422 = add nuw nsw i64 %tmp385, 192
  %tmp423 = getelementptr inbounds double, double* %arg1, i64 %tmp422
  %tmp424 = load double, double* %tmp423, align 8, !tbaa !2
  %tmp425 = fmul double %tmp421, %tmp424
  %tmp426 = fadd double %tmp420, %tmp425
  %tmp427 = load double, double* %tmp27, align 8, !tbaa !2
  %tmp428 = add nuw nsw i64 %tmp385, 224
  %tmp429 = getelementptr inbounds double, double* %arg1, i64 %tmp428
  %tmp430 = load double, double* %tmp429, align 8, !tbaa !2
  %tmp431 = fmul double %tmp427, %tmp430
  %tmp432 = fadd double %tmp426, %tmp431
  %tmp433 = load double, double* %tmp29, align 8, !tbaa !2
  %tmp434 = add nuw nsw i64 %tmp385, 256
  %tmp435 = getelementptr inbounds double, double* %arg1, i64 %tmp434
  %tmp436 = load double, double* %tmp435, align 8, !tbaa !2
  %tmp437 = fmul double %tmp433, %tmp436
  %tmp438 = fadd double %tmp432, %tmp437
  %tmp439 = load double, double* %tmp31, align 8, !tbaa !2
  %tmp440 = add nuw nsw i64 %tmp385, 288
  %tmp441 = getelementptr inbounds double, double* %arg1, i64 %tmp440
  %tmp442 = load double, double* %tmp441, align 8, !tbaa !2
  %tmp443 = fmul double %tmp439, %tmp442
  %tmp444 = fadd double %tmp438, %tmp443
  %tmp445 = load double, double* %tmp33, align 8, !tbaa !2
  %tmp446 = add nuw nsw i64 %tmp385, 320
  %tmp447 = getelementptr inbounds double, double* %arg1, i64 %tmp446
  %tmp448 = load double, double* %tmp447, align 8, !tbaa !2
  %tmp449 = fmul double %tmp445, %tmp448
  %tmp450 = fadd double %tmp444, %tmp449
  %tmp451 = load double, double* %tmp35, align 8, !tbaa !2
  %tmp452 = add nuw nsw i64 %tmp385, 352
  %tmp453 = getelementptr inbounds double, double* %arg1, i64 %tmp452
  %tmp454 = load double, double* %tmp453, align 8, !tbaa !2
  %tmp455 = fmul double %tmp451, %tmp454
  %tmp456 = fadd double %tmp450, %tmp455
  %tmp457 = load double, double* %tmp37, align 8, !tbaa !2
  %tmp458 = add nuw nsw i64 %tmp385, 384
  %tmp459 = getelementptr inbounds double, double* %arg1, i64 %tmp458
  %tmp460 = load double, double* %tmp459, align 8, !tbaa !2
  %tmp461 = fmul double %tmp457, %tmp460
  %tmp462 = fadd double %tmp456, %tmp461
  %tmp463 = load double, double* %tmp39, align 8, !tbaa !2
  %tmp464 = add nuw nsw i64 %tmp385, 416
  %tmp465 = getelementptr inbounds double, double* %arg1, i64 %tmp464
  %tmp466 = load double, double* %tmp465, align 8, !tbaa !2
  %tmp467 = fmul double %tmp463, %tmp466
  %tmp468 = fadd double %tmp462, %tmp467
  %tmp469 = load double, double* %tmp41, align 8, !tbaa !2
  %tmp470 = add nuw nsw i64 %tmp385, 448
  %tmp471 = getelementptr inbounds double, double* %arg1, i64 %tmp470
  %tmp472 = load double, double* %tmp471, align 8, !tbaa !2
  %tmp473 = fmul double %tmp469, %tmp472
  %tmp474 = fadd double %tmp468, %tmp473
  %tmp475 = load double, double* %tmp43, align 8, !tbaa !2
  %tmp476 = add nuw nsw i64 %tmp385, 480
  %tmp477 = getelementptr inbounds double, double* %arg1, i64 %tmp476
  %tmp478 = load double, double* %tmp477, align 8, !tbaa !2
  %tmp479 = fmul double %tmp475, %tmp478
  %tmp480 = fadd double %tmp474, %tmp479
  %tmp481 = load double, double* %tmp45, align 8, !tbaa !2
  %tmp482 = add nuw nsw i64 %tmp385, 512
  %tmp483 = getelementptr inbounds double, double* %arg1, i64 %tmp482
  %tmp484 = load double, double* %tmp483, align 8, !tbaa !2
  %tmp485 = fmul double %tmp481, %tmp484
  %tmp486 = fadd double %tmp480, %tmp485
  %tmp487 = load double, double* %tmp47, align 8, !tbaa !2
  %tmp488 = add nuw nsw i64 %tmp385, 544
  %tmp489 = getelementptr inbounds double, double* %arg1, i64 %tmp488
  %tmp490 = load double, double* %tmp489, align 8, !tbaa !2
  %tmp491 = fmul double %tmp487, %tmp490
  %tmp492 = fadd double %tmp486, %tmp491
  %tmp493 = load double, double* %tmp49, align 8, !tbaa !2
  %tmp494 = add nuw nsw i64 %tmp385, 576
  %tmp495 = getelementptr inbounds double, double* %arg1, i64 %tmp494
  %tmp496 = load double, double* %tmp495, align 8, !tbaa !2
  %tmp497 = fmul double %tmp493, %tmp496
  %tmp498 = fadd double %tmp492, %tmp497
  %tmp499 = load double, double* %tmp51, align 8, !tbaa !2
  %tmp500 = add nuw nsw i64 %tmp385, 608
  %tmp501 = getelementptr inbounds double, double* %arg1, i64 %tmp500
  %tmp502 = load double, double* %tmp501, align 8, !tbaa !2
  %tmp503 = fmul double %tmp499, %tmp502
  %tmp504 = fadd double %tmp498, %tmp503
  %tmp505 = load double, double* %tmp53, align 8, !tbaa !2
  %tmp506 = add nuw nsw i64 %tmp385, 640
  %tmp507 = getelementptr inbounds double, double* %arg1, i64 %tmp506
  %tmp508 = load double, double* %tmp507, align 8, !tbaa !2
  %tmp509 = fmul double %tmp505, %tmp508
  %tmp510 = fadd double %tmp504, %tmp509
  %tmp511 = load double, double* %tmp55, align 8, !tbaa !2
  %tmp512 = add nuw nsw i64 %tmp385, 672
  %tmp513 = getelementptr inbounds double, double* %arg1, i64 %tmp512
  %tmp514 = load double, double* %tmp513, align 8, !tbaa !2
  %tmp515 = fmul double %tmp511, %tmp514
  %tmp516 = fadd double %tmp510, %tmp515
  %tmp517 = load double, double* %tmp57, align 8, !tbaa !2
  %tmp518 = add nuw nsw i64 %tmp385, 704
  %tmp519 = getelementptr inbounds double, double* %arg1, i64 %tmp518
  %tmp520 = load double, double* %tmp519, align 8, !tbaa !2
  %tmp521 = fmul double %tmp517, %tmp520
  %tmp522 = fadd double %tmp516, %tmp521
  %tmp523 = load double, double* %tmp59, align 8, !tbaa !2
  %tmp524 = add nuw nsw i64 %tmp385, 736
  %tmp525 = getelementptr inbounds double, double* %arg1, i64 %tmp524
  %tmp526 = load double, double* %tmp525, align 8, !tbaa !2
  %tmp527 = fmul double %tmp523, %tmp526
  %tmp528 = fadd double %tmp522, %tmp527
  %tmp529 = load double, double* %tmp61, align 8, !tbaa !2
  %tmp530 = add nuw nsw i64 %tmp385, 768
  %tmp531 = getelementptr inbounds double, double* %arg1, i64 %tmp530
  %tmp532 = load double, double* %tmp531, align 8, !tbaa !2
  %tmp533 = fmul double %tmp529, %tmp532
  %tmp534 = fadd double %tmp528, %tmp533
  %tmp535 = load double, double* %tmp63, align 8, !tbaa !2
  %tmp536 = add nuw nsw i64 %tmp385, 800
  %tmp537 = getelementptr inbounds double, double* %arg1, i64 %tmp536
  %tmp538 = load double, double* %tmp537, align 8, !tbaa !2
  %tmp539 = fmul double %tmp535, %tmp538
  %tmp540 = fadd double %tmp534, %tmp539
  %tmp541 = load double, double* %tmp65, align 8, !tbaa !2
  %tmp542 = add nuw nsw i64 %tmp385, 832
  %tmp543 = getelementptr inbounds double, double* %arg1, i64 %tmp542
  %tmp544 = load double, double* %tmp543, align 8, !tbaa !2
  %tmp545 = fmul double %tmp541, %tmp544
  %tmp546 = fadd double %tmp540, %tmp545
  %tmp547 = load double, double* %tmp67, align 8, !tbaa !2
  %tmp548 = add nuw nsw i64 %tmp385, 864
  %tmp549 = getelementptr inbounds double, double* %arg1, i64 %tmp548
  %tmp550 = load double, double* %tmp549, align 8, !tbaa !2
  %tmp551 = fmul double %tmp547, %tmp550
  %tmp552 = fadd double %tmp546, %tmp551
  %tmp553 = load double, double* %tmp69, align 8, !tbaa !2
  %tmp554 = add nuw nsw i64 %tmp385, 896
  %tmp555 = getelementptr inbounds double, double* %arg1, i64 %tmp554
  %tmp556 = load double, double* %tmp555, align 8, !tbaa !2
  %tmp557 = fmul double %tmp553, %tmp556
  %tmp558 = fadd double %tmp552, %tmp557
  %tmp559 = load double, double* %tmp71, align 8, !tbaa !2
  %tmp560 = add nuw nsw i64 %tmp385, 928
  %tmp561 = getelementptr inbounds double, double* %arg1, i64 %tmp560
  %tmp562 = load double, double* %tmp561, align 8, !tbaa !2
  %tmp563 = fmul double %tmp559, %tmp562
  %tmp564 = fadd double %tmp558, %tmp563
  %tmp565 = load double, double* %tmp73, align 8, !tbaa !2
  %tmp566 = add nuw nsw i64 %tmp385, 960
  %tmp567 = getelementptr inbounds double, double* %arg1, i64 %tmp566
  %tmp568 = load double, double* %tmp567, align 8, !tbaa !2
  %tmp569 = fmul double %tmp565, %tmp568
  %tmp570 = fadd double %tmp564, %tmp569
  %tmp571 = load double, double* %tmp75, align 8, !tbaa !2
  %tmp572 = add nuw nsw i64 %tmp385, 992
  %tmp573 = getelementptr inbounds double, double* %arg1, i64 %tmp572
  %tmp574 = load double, double* %tmp573, align 8, !tbaa !2
  %tmp575 = fmul double %tmp571, %tmp574
  %tmp576 = fadd double %tmp570, %tmp575
  %tmp577 = add nuw nsw i64 %tmp385, %tmp12
  %tmp578 = getelementptr inbounds double, double* %arg2, i64 %tmp577
  store double %tmp576, double* %tmp578, align 8, !tbaa !2
  %tmp579 = add nuw nsw i64 %tmp385, 1
  %tmp580 = icmp eq i64 %tmp579, 32
  br i1 %tmp580, label %bb581, label %bb384, !llvm.loop !16

bb581:                                            ; preds = %bb384, %bb186
  %tmp582 = add nuw nsw i64 %tmp7, 1
  %tmp583 = icmp eq i64 %tmp582, 32
  br i1 %tmp583, label %bb584, label %bb6

bb584:                                            ; preds = %bb581
  %tmp585 = add nuw nsw i32 %tmp5, 1
  %tmp586 = icmp eq i32 %tmp585, 3
  br i1 %tmp586, label %bb3, label %bb4
}

; Function Attrs: nounwind uwtable
define internal dso_local void @run_benchmark(i8* %arg) #1 {
bb:
  %tmp = bitcast i8* %arg to double*
  %tmp1 = getelementptr inbounds i8, i8* %arg, i64 8192
  %tmp2 = bitcast i8* %tmp1 to double*
  %tmp3 = getelementptr inbounds i8, i8* %arg, i64 16384
  %tmp4 = bitcast i8* %tmp3 to double*
  tail call void @gemm(double* %tmp, double* nonnull %tmp2, double* nonnull %tmp4) #9
  ret void
}

; Function Attrs: nounwind uwtable
define internal dso_local void @input_to_data(i32 %arg, i8* %arg1) #1 {
bb:
  tail call void @llvm.memset.p0i8.i64(i8* %arg1, i8 0, i64 24576, i32 1, i1 false)
  %tmp = tail call i8* @readfile(i32 %arg) #9
  %tmp2 = tail call i8* @find_section_start(i8* %tmp, i32 1) #9
  %tmp3 = bitcast i8* %arg1 to double*
  %tmp4 = tail call i32 @parse_double_array(i8* %tmp2, double* %tmp3, i32 1024) #9
  %tmp5 = tail call i8* @find_section_start(i8* %tmp, i32 2) #9
  %tmp6 = getelementptr inbounds i8, i8* %arg1, i64 8192
  %tmp7 = bitcast i8* %tmp6 to double*
  %tmp8 = tail call i32 @parse_double_array(i8* %tmp5, double* nonnull %tmp7, i32 1024) #9
  tail call void @free(i8* %tmp) #9
  ret void
}

; Function Attrs: argmemonly nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i32, i1) #2

; Function Attrs: nounwind
declare void @free(i8* nocapture) local_unnamed_addr #3

; Function Attrs: nounwind uwtable
define internal dso_local void @data_to_input(i32 %arg, i8* %arg1) #1 {
bb:
  %tmp = tail call i32 @write_section_header(i32 %arg) #9
  %tmp2 = bitcast i8* %arg1 to double*
  %tmp3 = tail call i32 @write_double_array(i32 %arg, double* %tmp2, i32 1024) #9
  %tmp4 = tail call i32 @write_section_header(i32 %arg) #9
  %tmp5 = getelementptr inbounds i8, i8* %arg1, i64 8192
  %tmp6 = bitcast i8* %tmp5 to double*
  %tmp7 = tail call i32 @write_double_array(i32 %arg, double* nonnull %tmp6, i32 1024) #9
  ret void
}

; Function Attrs: nounwind uwtable
define internal dso_local void @output_to_data(i32 %arg, i8* %arg1) #1 {
bb:
  %tmp = tail call i8* @readfile(i32 %arg) #9
  %tmp2 = tail call i8* @find_section_start(i8* %tmp, i32 1) #9
  %tmp3 = getelementptr inbounds i8, i8* %arg1, i64 16384
  %tmp4 = bitcast i8* %tmp3 to double*
  %tmp5 = tail call i32 @parse_double_array(i8* %tmp2, double* nonnull %tmp4, i32 1024) #9
  tail call void @free(i8* %tmp) #9
  ret void
}

; Function Attrs: nounwind uwtable
define internal dso_local void @data_to_output(i32 %arg, i8* %arg1) #1 {
bb:
  %tmp = tail call i32 @write_section_header(i32 %arg) #9
  %tmp2 = getelementptr inbounds i8, i8* %arg1, i64 16384
  %tmp3 = bitcast i8* %tmp2 to double*
  %tmp4 = tail call i32 @write_double_array(i32 %arg, double* nonnull %tmp3, i32 1024) #9
  ret void
}

; Function Attrs: norecurse nounwind readonly uwtable
define internal dso_local i32 @check_data(i8* nocapture readonly %arg, i8* nocapture readonly %arg1) #4 {
bb:
  %tmp = getelementptr inbounds i8, i8* %arg, i64 16384
  %tmp2 = bitcast i8* %tmp to [1024 x double]*
  %tmp3 = getelementptr inbounds i8, i8* %arg1, i64 16384
  %tmp4 = bitcast i8* %tmp3 to [1024 x double]*
  br label %bb5

bb5:                                              ; preds = %bb5, %bb
  %tmp6 = phi i64 [ 0, %bb ], [ %tmp220, %bb5 ]
  %tmp7 = phi i32 [ 0, %bb ], [ %tmp219, %bb5 ]
  %tmp8 = shl i64 %tmp6, 5
  %tmp9 = insertelement <2 x i32> <i32 undef, i32 0>, i32 %tmp7, i32 0
  %tmp10 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp8
  %tmp11 = bitcast double* %tmp10 to <2 x double>*
  %tmp12 = load <2 x double>, <2 x double>* %tmp11, align 8, !tbaa !2
  %tmp13 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp8
  %tmp14 = bitcast double* %tmp13 to <2 x double>*
  %tmp15 = load <2 x double>, <2 x double>* %tmp14, align 8, !tbaa !2
  %tmp16 = fsub <2 x double> %tmp12, %tmp15
  %tmp17 = fcmp olt <2 x double> %tmp16, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp18 = fcmp ogt <2 x double> %tmp16, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp19 = or <2 x i1> %tmp17, %tmp18
  %tmp20 = zext <2 x i1> %tmp19 to <2 x i32>
  %tmp21 = or <2 x i32> %tmp9, %tmp20
  %tmp22 = or i64 %tmp8, 2
  %tmp23 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp22
  %tmp24 = bitcast double* %tmp23 to <2 x double>*
  %tmp25 = load <2 x double>, <2 x double>* %tmp24, align 8, !tbaa !2
  %tmp26 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp22
  %tmp27 = bitcast double* %tmp26 to <2 x double>*
  %tmp28 = load <2 x double>, <2 x double>* %tmp27, align 8, !tbaa !2
  %tmp29 = fsub <2 x double> %tmp25, %tmp28
  %tmp30 = fcmp olt <2 x double> %tmp29, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp31 = fcmp ogt <2 x double> %tmp29, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp32 = or <2 x i1> %tmp30, %tmp31
  %tmp33 = zext <2 x i1> %tmp32 to <2 x i32>
  %tmp34 = or <2 x i32> %tmp21, %tmp33
  %tmp35 = or i64 %tmp8, 4
  %tmp36 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp35
  %tmp37 = bitcast double* %tmp36 to <2 x double>*
  %tmp38 = load <2 x double>, <2 x double>* %tmp37, align 8, !tbaa !2
  %tmp39 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp35
  %tmp40 = bitcast double* %tmp39 to <2 x double>*
  %tmp41 = load <2 x double>, <2 x double>* %tmp40, align 8, !tbaa !2
  %tmp42 = fsub <2 x double> %tmp38, %tmp41
  %tmp43 = fcmp olt <2 x double> %tmp42, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp44 = fcmp ogt <2 x double> %tmp42, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp45 = or <2 x i1> %tmp43, %tmp44
  %tmp46 = zext <2 x i1> %tmp45 to <2 x i32>
  %tmp47 = or <2 x i32> %tmp34, %tmp46
  %tmp48 = or i64 %tmp8, 6
  %tmp49 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp48
  %tmp50 = bitcast double* %tmp49 to <2 x double>*
  %tmp51 = load <2 x double>, <2 x double>* %tmp50, align 8, !tbaa !2
  %tmp52 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp48
  %tmp53 = bitcast double* %tmp52 to <2 x double>*
  %tmp54 = load <2 x double>, <2 x double>* %tmp53, align 8, !tbaa !2
  %tmp55 = fsub <2 x double> %tmp51, %tmp54
  %tmp56 = fcmp olt <2 x double> %tmp55, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp57 = fcmp ogt <2 x double> %tmp55, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp58 = or <2 x i1> %tmp56, %tmp57
  %tmp59 = zext <2 x i1> %tmp58 to <2 x i32>
  %tmp60 = or <2 x i32> %tmp47, %tmp59
  %tmp61 = or i64 %tmp8, 8
  %tmp62 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp61
  %tmp63 = bitcast double* %tmp62 to <2 x double>*
  %tmp64 = load <2 x double>, <2 x double>* %tmp63, align 8, !tbaa !2
  %tmp65 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp61
  %tmp66 = bitcast double* %tmp65 to <2 x double>*
  %tmp67 = load <2 x double>, <2 x double>* %tmp66, align 8, !tbaa !2
  %tmp68 = fsub <2 x double> %tmp64, %tmp67
  %tmp69 = fcmp olt <2 x double> %tmp68, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp70 = fcmp ogt <2 x double> %tmp68, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp71 = or <2 x i1> %tmp69, %tmp70
  %tmp72 = zext <2 x i1> %tmp71 to <2 x i32>
  %tmp73 = or <2 x i32> %tmp60, %tmp72
  %tmp74 = or i64 %tmp8, 10
  %tmp75 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp74
  %tmp76 = bitcast double* %tmp75 to <2 x double>*
  %tmp77 = load <2 x double>, <2 x double>* %tmp76, align 8, !tbaa !2
  %tmp78 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp74
  %tmp79 = bitcast double* %tmp78 to <2 x double>*
  %tmp80 = load <2 x double>, <2 x double>* %tmp79, align 8, !tbaa !2
  %tmp81 = fsub <2 x double> %tmp77, %tmp80
  %tmp82 = fcmp olt <2 x double> %tmp81, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp83 = fcmp ogt <2 x double> %tmp81, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp84 = or <2 x i1> %tmp82, %tmp83
  %tmp85 = zext <2 x i1> %tmp84 to <2 x i32>
  %tmp86 = or <2 x i32> %tmp73, %tmp85
  %tmp87 = or i64 %tmp8, 12
  %tmp88 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp87
  %tmp89 = bitcast double* %tmp88 to <2 x double>*
  %tmp90 = load <2 x double>, <2 x double>* %tmp89, align 8, !tbaa !2
  %tmp91 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp87
  %tmp92 = bitcast double* %tmp91 to <2 x double>*
  %tmp93 = load <2 x double>, <2 x double>* %tmp92, align 8, !tbaa !2
  %tmp94 = fsub <2 x double> %tmp90, %tmp93
  %tmp95 = fcmp olt <2 x double> %tmp94, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp96 = fcmp ogt <2 x double> %tmp94, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp97 = or <2 x i1> %tmp95, %tmp96
  %tmp98 = zext <2 x i1> %tmp97 to <2 x i32>
  %tmp99 = or <2 x i32> %tmp86, %tmp98
  %tmp100 = or i64 %tmp8, 14
  %tmp101 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp100
  %tmp102 = bitcast double* %tmp101 to <2 x double>*
  %tmp103 = load <2 x double>, <2 x double>* %tmp102, align 8, !tbaa !2
  %tmp104 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp100
  %tmp105 = bitcast double* %tmp104 to <2 x double>*
  %tmp106 = load <2 x double>, <2 x double>* %tmp105, align 8, !tbaa !2
  %tmp107 = fsub <2 x double> %tmp103, %tmp106
  %tmp108 = fcmp olt <2 x double> %tmp107, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp109 = fcmp ogt <2 x double> %tmp107, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp110 = or <2 x i1> %tmp108, %tmp109
  %tmp111 = zext <2 x i1> %tmp110 to <2 x i32>
  %tmp112 = or <2 x i32> %tmp99, %tmp111
  %tmp113 = or i64 %tmp8, 16
  %tmp114 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp113
  %tmp115 = bitcast double* %tmp114 to <2 x double>*
  %tmp116 = load <2 x double>, <2 x double>* %tmp115, align 8, !tbaa !2
  %tmp117 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp113
  %tmp118 = bitcast double* %tmp117 to <2 x double>*
  %tmp119 = load <2 x double>, <2 x double>* %tmp118, align 8, !tbaa !2
  %tmp120 = fsub <2 x double> %tmp116, %tmp119
  %tmp121 = fcmp olt <2 x double> %tmp120, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp122 = fcmp ogt <2 x double> %tmp120, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp123 = or <2 x i1> %tmp121, %tmp122
  %tmp124 = zext <2 x i1> %tmp123 to <2 x i32>
  %tmp125 = or <2 x i32> %tmp112, %tmp124
  %tmp126 = or i64 %tmp8, 18
  %tmp127 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp126
  %tmp128 = bitcast double* %tmp127 to <2 x double>*
  %tmp129 = load <2 x double>, <2 x double>* %tmp128, align 8, !tbaa !2
  %tmp130 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp126
  %tmp131 = bitcast double* %tmp130 to <2 x double>*
  %tmp132 = load <2 x double>, <2 x double>* %tmp131, align 8, !tbaa !2
  %tmp133 = fsub <2 x double> %tmp129, %tmp132
  %tmp134 = fcmp olt <2 x double> %tmp133, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp135 = fcmp ogt <2 x double> %tmp133, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp136 = or <2 x i1> %tmp134, %tmp135
  %tmp137 = zext <2 x i1> %tmp136 to <2 x i32>
  %tmp138 = or <2 x i32> %tmp125, %tmp137
  %tmp139 = or i64 %tmp8, 20
  %tmp140 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp139
  %tmp141 = bitcast double* %tmp140 to <2 x double>*
  %tmp142 = load <2 x double>, <2 x double>* %tmp141, align 8, !tbaa !2
  %tmp143 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp139
  %tmp144 = bitcast double* %tmp143 to <2 x double>*
  %tmp145 = load <2 x double>, <2 x double>* %tmp144, align 8, !tbaa !2
  %tmp146 = fsub <2 x double> %tmp142, %tmp145
  %tmp147 = fcmp olt <2 x double> %tmp146, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp148 = fcmp ogt <2 x double> %tmp146, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp149 = or <2 x i1> %tmp147, %tmp148
  %tmp150 = zext <2 x i1> %tmp149 to <2 x i32>
  %tmp151 = or <2 x i32> %tmp138, %tmp150
  %tmp152 = or i64 %tmp8, 22
  %tmp153 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp152
  %tmp154 = bitcast double* %tmp153 to <2 x double>*
  %tmp155 = load <2 x double>, <2 x double>* %tmp154, align 8, !tbaa !2
  %tmp156 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp152
  %tmp157 = bitcast double* %tmp156 to <2 x double>*
  %tmp158 = load <2 x double>, <2 x double>* %tmp157, align 8, !tbaa !2
  %tmp159 = fsub <2 x double> %tmp155, %tmp158
  %tmp160 = fcmp olt <2 x double> %tmp159, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp161 = fcmp ogt <2 x double> %tmp159, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp162 = or <2 x i1> %tmp160, %tmp161
  %tmp163 = zext <2 x i1> %tmp162 to <2 x i32>
  %tmp164 = or <2 x i32> %tmp151, %tmp163
  %tmp165 = or i64 %tmp8, 24
  %tmp166 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp165
  %tmp167 = bitcast double* %tmp166 to <2 x double>*
  %tmp168 = load <2 x double>, <2 x double>* %tmp167, align 8, !tbaa !2
  %tmp169 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp165
  %tmp170 = bitcast double* %tmp169 to <2 x double>*
  %tmp171 = load <2 x double>, <2 x double>* %tmp170, align 8, !tbaa !2
  %tmp172 = fsub <2 x double> %tmp168, %tmp171
  %tmp173 = fcmp olt <2 x double> %tmp172, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp174 = fcmp ogt <2 x double> %tmp172, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp175 = or <2 x i1> %tmp173, %tmp174
  %tmp176 = zext <2 x i1> %tmp175 to <2 x i32>
  %tmp177 = or <2 x i32> %tmp164, %tmp176
  %tmp178 = or i64 %tmp8, 26
  %tmp179 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp178
  %tmp180 = bitcast double* %tmp179 to <2 x double>*
  %tmp181 = load <2 x double>, <2 x double>* %tmp180, align 8, !tbaa !2
  %tmp182 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp178
  %tmp183 = bitcast double* %tmp182 to <2 x double>*
  %tmp184 = load <2 x double>, <2 x double>* %tmp183, align 8, !tbaa !2
  %tmp185 = fsub <2 x double> %tmp181, %tmp184
  %tmp186 = fcmp olt <2 x double> %tmp185, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp187 = fcmp ogt <2 x double> %tmp185, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp188 = or <2 x i1> %tmp186, %tmp187
  %tmp189 = zext <2 x i1> %tmp188 to <2 x i32>
  %tmp190 = or <2 x i32> %tmp177, %tmp189
  %tmp191 = or i64 %tmp8, 28
  %tmp192 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp191
  %tmp193 = bitcast double* %tmp192 to <2 x double>*
  %tmp194 = load <2 x double>, <2 x double>* %tmp193, align 8, !tbaa !2
  %tmp195 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp191
  %tmp196 = bitcast double* %tmp195 to <2 x double>*
  %tmp197 = load <2 x double>, <2 x double>* %tmp196, align 8, !tbaa !2
  %tmp198 = fsub <2 x double> %tmp194, %tmp197
  %tmp199 = fcmp olt <2 x double> %tmp198, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp200 = fcmp ogt <2 x double> %tmp198, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp201 = or <2 x i1> %tmp199, %tmp200
  %tmp202 = zext <2 x i1> %tmp201 to <2 x i32>
  %tmp203 = or <2 x i32> %tmp190, %tmp202
  %tmp204 = or i64 %tmp8, 30
  %tmp205 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp2, i64 0, i64 %tmp204
  %tmp206 = bitcast double* %tmp205 to <2 x double>*
  %tmp207 = load <2 x double>, <2 x double>* %tmp206, align 8, !tbaa !2
  %tmp208 = getelementptr inbounds [1024 x double], [1024 x double]* %tmp4, i64 0, i64 %tmp204
  %tmp209 = bitcast double* %tmp208 to <2 x double>*
  %tmp210 = load <2 x double>, <2 x double>* %tmp209, align 8, !tbaa !2
  %tmp211 = fsub <2 x double> %tmp207, %tmp210
  %tmp212 = fcmp olt <2 x double> %tmp211, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp213 = fcmp ogt <2 x double> %tmp211, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp214 = or <2 x i1> %tmp212, %tmp213
  %tmp215 = zext <2 x i1> %tmp214 to <2 x i32>
  %tmp216 = or <2 x i32> %tmp203, %tmp215
  %tmp217 = shufflevector <2 x i32> %tmp216, <2 x i32> undef, <2 x i32> <i32 1, i32 undef>
  %tmp218 = or <2 x i32> %tmp216, %tmp217
  %tmp219 = extractelement <2 x i32> %tmp218, i32 0
  %tmp220 = add nuw nsw i64 %tmp6, 1
  %tmp221 = icmp eq i64 %tmp220, 32
  br i1 %tmp221, label %bb222, label %bb5

bb222:                                            ; preds = %bb5
  %tmp223 = icmp eq i32 %tmp219, 0
  %tmp224 = zext i1 %tmp223 to i32
  ret i32 %tmp224
}

; Function Attrs: nounwind uwtable
define internal dso_local noalias i8* @readfile(i32 %arg) #1 {
bb:
  %tmp = alloca %struct.stat, align 8
  %tmp1 = bitcast %struct.stat* %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 144, i8* nonnull %tmp1) #9
  %tmp2 = icmp sgt i32 %arg, 1
  br i1 %tmp2, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 40, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #10
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = call i32 @__fxstat(i32 1, i32 %arg, %struct.stat* nonnull %tmp) #9
  %tmp6 = icmp eq i32 %tmp5, 0
  br i1 %tmp6, label %bb8, label %bb7

bb7:                                              ; preds = %bb4
  call void @__assert_fail(i8* getelementptr inbounds ([51 x i8], [51 x i8]* @.str.4, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 41, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #10
  unreachable

bb8:                                              ; preds = %bb4
  %tmp9 = getelementptr inbounds %struct.stat, %struct.stat* %tmp, i64 0, i32 8
  %tmp10 = load i64, i64* %tmp9, align 8, !tbaa !17
  %tmp11 = icmp sgt i64 %tmp10, 0
  br i1 %tmp11, label %bb13, label %bb12

bb12:                                             ; preds = %bb8
  call void @__assert_fail(i8* getelementptr inbounds ([25 x i8], [25 x i8]* @.str.6, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 43, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #10
  unreachable

bb13:                                             ; preds = %bb8
  %tmp14 = add nsw i64 %tmp10, 1
  %tmp15 = call noalias i8* @malloc(i64 %tmp14) #9
  br label %bb18

bb16:                                             ; preds = %bb18
  %tmp17 = icmp sgt i64 %tmp10, %tmp24
  br i1 %tmp17, label %bb18, label %bb26

bb18:                                             ; preds = %bb16, %bb13
  %tmp19 = phi i64 [ 0, %bb13 ], [ %tmp24, %bb16 ]
  %tmp20 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp19
  %tmp21 = sub nsw i64 %tmp10, %tmp19
  %tmp22 = call i64 @read(i32 %arg, i8* %tmp20, i64 %tmp21) #9
  %tmp23 = icmp sgt i64 %tmp22, -1
  %tmp24 = add nsw i64 %tmp22, %tmp19
  br i1 %tmp23, label %bb16, label %bb25

bb25:                                             ; preds = %bb18
  call void @__assert_fail(i8* getelementptr inbounds ([29 x i8], [29 x i8]* @.str.8, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 48, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #10
  unreachable

bb26:                                             ; preds = %bb16
  %tmp27 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp10
  store i8 0, i8* %tmp27, align 1, !tbaa !22
  %tmp28 = call i32 @close(i32 %arg) #9
  call void @llvm.lifetime.end.p0i8(i64 144, i8* nonnull %tmp1) #9
  ret i8* %tmp15
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64, i8* nocapture) #2

; Function Attrs: noreturn nounwind
declare void @__assert_fail(i8*, i8*, i32, i8*) local_unnamed_addr #5

; Function Attrs: nounwind
declare i32 @__fxstat(i32, i32, %struct.stat*) local_unnamed_addr #3

; Function Attrs: nounwind
declare noalias i8* @malloc(i64) local_unnamed_addr #3

declare i64 @read(i32, i8* nocapture, i64) local_unnamed_addr #6

declare i32 @close(i32) local_unnamed_addr #6

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64, i8* nocapture) #2

; Function Attrs: nounwind uwtable
define internal dso_local i8* @find_section_start(i8* readonly %arg, i32 %arg1) #1 {
bb:
  %tmp = icmp sgt i32 %arg1, -1
  br i1 %tmp, label %bb3, label %bb2

bb2:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([33 x i8], [33 x i8]* @.str.10, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 59, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.find_section_start, i64 0, i64 0)) #10
  unreachable

bb3:                                              ; preds = %bb
  %tmp4 = icmp eq i32 %arg1, 0
  br i1 %tmp4, label %bb34, label %bb5

bb5:                                              ; preds = %bb3
  %tmp6 = load i8, i8* %arg, align 1, !tbaa !22
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
  %tmp13 = load i8, i8* %tmp12, align 1, !tbaa !22
  br label %bb24

bb14:                                             ; preds = %bb7
  %tmp15 = getelementptr inbounds i8, i8* %tmp10, i64 1
  %tmp16 = load i8, i8* %tmp15, align 1, !tbaa !22
  %tmp17 = icmp eq i8 %tmp16, 37
  br i1 %tmp17, label %bb18, label %bb24

bb18:                                             ; preds = %bb14
  %tmp19 = getelementptr inbounds i8, i8* %tmp10, i64 2
  %tmp20 = load i8, i8* %tmp19, align 1, !tbaa !22
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

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_string(i8* readonly %arg, i8* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp eq i8* %arg, null
  br i1 %tmp, label %bb3, label %bb4

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 79, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.parse_string, i64 0, i64 0)) #10
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp slt i32 %arg2, 0
  br i1 %tmp5, label %bb8, label %bb6

bb6:                                              ; preds = %bb4
  %tmp7 = sext i32 %arg2 to i64
  tail call void @llvm.memcpy.p0i8.p0i8.i64(i8* %arg1, i8* nonnull %arg, i64 %tmp7, i32 1, i1 false)
  br label %bb47

bb8:                                              ; preds = %bb4
  %tmp9 = load i8, i8* %arg, align 1, !tbaa !22
  %tmp10 = icmp eq i8 %tmp9, 0
  br i1 %tmp10, label %bb43, label %bb11

bb11:                                             ; preds = %bb8
  %tmp12 = getelementptr inbounds i8, i8* %arg, i64 1
  %tmp13 = load i8, i8* %tmp12, align 1, !tbaa !22
  br label %bb17

bb14:                                             ; preds = %bb31
  %tmp15 = load i8, i8* %tmp24, align 1, !tbaa !22
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
  %tmp29 = load i8, i8* %tmp28, align 1, !tbaa !22
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
  store i8 0, i8* %tmp46, align 1, !tbaa !22
  br label %bb47

bb47:                                             ; preds = %bb43, %bb6
  ret i32 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture writeonly, i8* nocapture readonly, i64, i32, i1) #2

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_uint8_t_array(i8* %arg, i8* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #9
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 132, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_uint8_t_array, i64 0, i64 0)) #10
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !23
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #9
  %tmp17 = trunc i64 %tmp16 to i8
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !23
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !22
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !23
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #11
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i8, i8* %arg1, i64 %tmp14
  store i8 %tmp17, i8* %tmp26, align 1, !tbaa !22
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #12
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !22
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #12
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !22
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #9
  ret i32 0
}

; Function Attrs: nounwind
declare i8* @strtok(i8*, i8* nocapture readonly) local_unnamed_addr #3

; Function Attrs: nounwind
declare i64 @strtol(i8* readonly, i8** nocapture, i32) local_unnamed_addr #3

; Function Attrs: nounwind
declare i32 @fprintf(%struct._IO_FILE* nocapture, i8* nocapture readonly, ...) local_unnamed_addr #3

; Function Attrs: argmemonly nounwind readonly
declare i64 @strlen(i8* nocapture) local_unnamed_addr #7

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_uint16_t_array(i8* %arg, i16* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #9
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 133, i8* getelementptr inbounds ([50 x i8], [50 x i8]* @__PRETTY_FUNCTION__.parse_uint16_t_array, i64 0, i64 0)) #10
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !23
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #9
  %tmp17 = trunc i64 %tmp16 to i16
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !23
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !22
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !23
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #11
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i16, i16* %arg1, i64 %tmp14
  store i16 %tmp17, i16* %tmp26, align 2, !tbaa !25
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #12
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !22
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #12
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !22
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #9
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_uint32_t_array(i8* %arg, i32* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #9
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 134, i8* getelementptr inbounds ([50 x i8], [50 x i8]* @__PRETTY_FUNCTION__.parse_uint32_t_array, i64 0, i64 0)) #10
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !23
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #9
  %tmp17 = trunc i64 %tmp16 to i32
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !23
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !22
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !23
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #11
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i32, i32* %arg1, i64 %tmp14
  store i32 %tmp17, i32* %tmp26, align 4, !tbaa !27
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #12
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !22
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #12
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !22
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #9
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_uint64_t_array(i8* %arg, i64* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #9
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 135, i8* getelementptr inbounds ([50 x i8], [50 x i8]* @__PRETTY_FUNCTION__.parse_uint64_t_array, i64 0, i64 0)) #10
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !23
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #9
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !23
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !22
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !23
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #11
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds i64, i64* %arg1, i64 %tmp14
  store i64 %tmp16, i64* %tmp25, align 8, !tbaa !28
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #12
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !22
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #12
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !22
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #9
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_int8_t_array(i8* %arg, i8* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #9
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 136, i8* getelementptr inbounds ([46 x i8], [46 x i8]* @__PRETTY_FUNCTION__.parse_int8_t_array, i64 0, i64 0)) #10
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !23
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #9
  %tmp17 = trunc i64 %tmp16 to i8
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !23
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !22
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !23
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #11
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i8, i8* %arg1, i64 %tmp14
  store i8 %tmp17, i8* %tmp26, align 1, !tbaa !22
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #12
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !22
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #12
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !22
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #9
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_int16_t_array(i8* %arg, i16* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #9
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 137, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_int16_t_array, i64 0, i64 0)) #10
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !23
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #9
  %tmp17 = trunc i64 %tmp16 to i16
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !23
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !22
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !23
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #11
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i16, i16* %arg1, i64 %tmp14
  store i16 %tmp17, i16* %tmp26, align 2, !tbaa !25
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #12
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !22
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #12
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !22
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #9
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_int32_t_array(i8* %arg, i32* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #9
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 138, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_int32_t_array, i64 0, i64 0)) #10
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !23
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #9
  %tmp17 = trunc i64 %tmp16 to i32
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !23
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !22
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !23
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #11
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i32, i32* %arg1, i64 %tmp14
  store i32 %tmp17, i32* %tmp26, align 4, !tbaa !27
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #12
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !22
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #12
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !22
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #9
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_int64_t_array(i8* %arg, i64* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #9
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 139, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_int64_t_array, i64 0, i64 0)) #10
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !23
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #9
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !23
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !22
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !23
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #11
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds i64, i64* %arg1, i64 %tmp14
  store i64 %tmp16, i64* %tmp25, align 8, !tbaa !28
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #12
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !22
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #12
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !22
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #9
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_float_array(i8* %arg, float* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #9
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 141, i8* getelementptr inbounds ([44 x i8], [44 x i8]* @__PRETTY_FUNCTION__.parse_float_array, i64 0, i64 0)) #10
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !23
  %tmp16 = call float @strtof(i8* nonnull %tmp15, i8** nonnull %tmp) #9
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !23
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !22
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !23
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #11
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds float, float* %arg1, i64 %tmp14
  store float %tmp16, float* %tmp25, align 4, !tbaa !29
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #12
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !22
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #12
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !22
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #9
  ret i32 0
}

; Function Attrs: nounwind
declare float @strtof(i8* readonly, i8** nocapture) local_unnamed_addr #3

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_double_array(i8* %arg, double* nocapture %arg1, i32 %arg2) #1 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #9
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 142, i8* getelementptr inbounds ([46 x i8], [46 x i8]* @__PRETTY_FUNCTION__.parse_double_array, i64 0, i64 0)) #10
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !23
  %tmp16 = call double @strtod(i8* nonnull %tmp15, i8** nonnull %tmp) #9
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !23
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !22
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !23
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #11
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds double, double* %arg1, i64 %tmp14
  store double %tmp16, double* %tmp25, align 8, !tbaa !2
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #12
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !22
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #9
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #12
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !22
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #9
  ret i32 0
}

; Function Attrs: nounwind
declare double @strtod(i8* readonly, i8** nocapture) local_unnamed_addr #3

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_string(i32 %arg, i8* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 147, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @__PRETTY_FUNCTION__.write_string, i64 0, i64 0)) #10
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp slt i32 %arg2, 0
  br i1 %tmp5, label %bb6, label %bb9

bb6:                                              ; preds = %bb4
  %tmp7 = tail call i64 @strlen(i8* %arg1) #12
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
  %tmp22 = tail call i64 @write(i32 %arg, i8* %tmp19, i64 %tmp21) #9
  %tmp23 = trunc i64 %tmp22 to i32
  %tmp24 = icmp sgt i32 %tmp23, -1
  %tmp25 = add nsw i32 %tmp17, %tmp23
  br i1 %tmp24, label %bb14, label %bb26

bb26:                                             ; preds = %bb16
  tail call void @__assert_fail(i8* getelementptr inbounds ([28 x i8], [28 x i8]* @.str.16, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 154, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @__PRETTY_FUNCTION__.write_string, i64 0, i64 0)) #10
  unreachable

bb27:                                             ; preds = %bb32, %bb13
  %tmp28 = tail call i64 @write(i32 %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0), i64 1) #9
  %tmp29 = trunc i64 %tmp28 to i32
  %tmp30 = icmp sgt i32 %tmp29, -1
  br i1 %tmp30, label %bb32, label %bb31

bb31:                                             ; preds = %bb27
  tail call void @__assert_fail(i8* getelementptr inbounds ([28 x i8], [28 x i8]* @.str.16, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 160, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @__PRETTY_FUNCTION__.write_string, i64 0, i64 0)) #10
  unreachable

bb32:                                             ; preds = %bb27
  %tmp33 = icmp eq i32 %tmp29, 0
  br i1 %tmp33, label %bb27, label %bb34

bb34:                                             ; preds = %bb32
  ret i32 0
}

declare i64 @write(i32, i8* nocapture readonly, i64) local_unnamed_addr #6

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_uint8_t_array(i32 %arg, i8* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 177, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_uint8_t_array, i64 0, i64 0)) #10
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
  %tmp11 = load i8, i8* %tmp10, align 1, !tbaa !22
  %tmp12 = zext i8 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.17, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: inlinehint nounwind uwtable
define internal void @fd_printf(i32 %arg, i8* nocapture readonly %arg1, ...) unnamed_addr #8 {
bb:
  %tmp = alloca [1 x %struct.__va_list_tag], align 16
  %tmp2 = alloca [256 x i8], align 16
  %tmp3 = bitcast [1 x %struct.__va_list_tag]* %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 24, i8* nonnull %tmp3) #9
  %tmp4 = getelementptr inbounds [256 x i8], [256 x i8]* %tmp2, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 256, i8* nonnull %tmp4) #9
  %tmp5 = getelementptr inbounds [1 x %struct.__va_list_tag], [1 x %struct.__va_list_tag]* %tmp, i64 0, i64 0
  call void @llvm.va_start(i8* nonnull %tmp3)
  %tmp6 = call i32 @vsnprintf(i8* nonnull %tmp4, i64 256, i8* %arg1, %struct.__va_list_tag* nonnull %tmp5) #9
  call void @llvm.va_end(i8* nonnull %tmp3)
  %tmp7 = icmp slt i32 %tmp6, 256
  br i1 %tmp7, label %bb9, label %bb8

bb8:                                              ; preds = %bb
  call void @__assert_fail(i8* getelementptr inbounds ([90 x i8], [90 x i8]* @.str.24, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 22, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.fd_printf, i64 0, i64 0)) #10
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
  %tmp20 = call i64 @write(i32 %arg, i8* nonnull %tmp17, i64 %tmp19) #9
  %tmp21 = trunc i64 %tmp20 to i32
  %tmp22 = icmp sgt i32 %tmp21, -1
  %tmp23 = add nsw i32 %tmp15, %tmp21
  br i1 %tmp22, label %bb12, label %bb24

bb24:                                             ; preds = %bb14
  call void @__assert_fail(i8* getelementptr inbounds ([28 x i8], [28 x i8]* @.str.16, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 26, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.fd_printf, i64 0, i64 0)) #10
  unreachable

bb25:                                             ; preds = %bb12, %bb9
  %tmp26 = phi i32 [ 0, %bb9 ], [ %tmp23, %bb12 ]
  %tmp27 = icmp eq i32 %tmp6, %tmp26
  br i1 %tmp27, label %bb29, label %bb28

bb28:                                             ; preds = %bb25
  call void @__assert_fail(i8* getelementptr inbounds ([50 x i8], [50 x i8]* @.str.26, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 29, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.fd_printf, i64 0, i64 0)) #10
  unreachable

bb29:                                             ; preds = %bb25
  call void @llvm.lifetime.end.p0i8(i64 256, i8* nonnull %tmp4) #9
  call void @llvm.lifetime.end.p0i8(i64 24, i8* nonnull %tmp3) #9
  ret void
}

; Function Attrs: nounwind
declare void @llvm.va_start(i8*) #9

; Function Attrs: nounwind
declare i32 @vsnprintf(i8* nocapture, i64, i8* nocapture readonly, %struct.__va_list_tag*) local_unnamed_addr #3

; Function Attrs: nounwind
declare void @llvm.va_end(i8*) #9

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_uint16_t_array(i32 %arg, i16* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 178, i8* getelementptr inbounds ([47 x i8], [47 x i8]* @__PRETTY_FUNCTION__.write_uint16_t_array, i64 0, i64 0)) #10
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
  %tmp11 = load i16, i16* %tmp10, align 2, !tbaa !25
  %tmp12 = zext i16 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.17, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_uint32_t_array(i32 %arg, i32* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 179, i8* getelementptr inbounds ([47 x i8], [47 x i8]* @__PRETTY_FUNCTION__.write_uint32_t_array, i64 0, i64 0)) #10
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
  %tmp11 = load i32, i32* %tmp10, align 4, !tbaa !27
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.17, i64 0, i64 0), i32 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_uint64_t_array(i32 %arg, i64* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 180, i8* getelementptr inbounds ([47 x i8], [47 x i8]* @__PRETTY_FUNCTION__.write_uint64_t_array, i64 0, i64 0)) #10
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
  %tmp11 = load i64, i64* %tmp10, align 8, !tbaa !28
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str.18, i64 0, i64 0), i64 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_int8_t_array(i32 %arg, i8* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 181, i8* getelementptr inbounds ([43 x i8], [43 x i8]* @__PRETTY_FUNCTION__.write_int8_t_array, i64 0, i64 0)) #10
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
  %tmp11 = load i8, i8* %tmp10, align 1, !tbaa !22
  %tmp12 = sext i8 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.19, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_int16_t_array(i32 %arg, i16* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 182, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_int16_t_array, i64 0, i64 0)) #10
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
  %tmp11 = load i16, i16* %tmp10, align 2, !tbaa !25
  %tmp12 = sext i16 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.19, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_int32_t_array(i32 %arg, i32* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 183, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_int32_t_array, i64 0, i64 0)) #10
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
  %tmp11 = load i32, i32* %tmp10, align 4, !tbaa !27
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.19, i64 0, i64 0), i32 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_int64_t_array(i32 %arg, i64* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 184, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_int64_t_array, i64 0, i64 0)) #10
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
  %tmp11 = load i64, i64* %tmp10, align 8, !tbaa !28
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str.20, i64 0, i64 0), i64 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_float_array(i32 %arg, float* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 186, i8* getelementptr inbounds ([41 x i8], [41 x i8]* @__PRETTY_FUNCTION__.write_float_array, i64 0, i64 0)) #10
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
  %tmp11 = load float, float* %tmp10, align 4, !tbaa !29
  %tmp12 = fpext float %tmp11 to double
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.21, i64 0, i64 0), double %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_double_array(i32 %arg, double* nocapture readonly %arg1, i32 %arg2) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 187, i8* getelementptr inbounds ([43 x i8], [43 x i8]* @__PRETTY_FUNCTION__.write_double_array, i64 0, i64 0)) #10
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
  %tmp11 = load double, double* %tmp10, align 8, !tbaa !2
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.21, i64 0, i64 0), double %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_section_header(i32 %arg) #1 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb2, label %bb1

bb1:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 190, i8* getelementptr inbounds ([30 x i8], [30 x i8]* @__PRETTY_FUNCTION__.write_section_header, i64 0, i64 0)) #10
  unreachable

bb2:                                              ; preds = %bb
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.22, i64 0, i64 0))
  ret i32 0
}

; Function Attrs: nounwind uwtable
define dso_local i32 @main(i32 %arg, i8** nocapture readonly %arg1) #1 {
bb:
  %tmp = icmp slt i32 %arg, 4
  br i1 %tmp, label %bb3, label %bb2

bb2:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([57 x i8], [57 x i8]* @.str.1.11, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.12, i64 0, i64 0), i32 21, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #10
  unreachable

bb3:                                              ; preds = %bb
  %tmp4 = icmp sgt i32 %arg, 1
  br i1 %tmp4, label %bb5, label %bb8

bb5:                                              ; preds = %bb3
  %tmp6 = getelementptr inbounds i8*, i8** %arg1, i64 1
  %tmp7 = load i8*, i8** %tmp6, align 8, !tbaa !23
  br label %bb8

bb8:                                              ; preds = %bb5, %bb3
  %tmp9 = phi i8* [ %tmp7, %bb5 ], [ getelementptr inbounds ([11 x i8], [11 x i8]* @.str.3, i64 0, i64 0), %bb3 ]
  %tmp10 = load i32, i32* @INPUT_SIZE, align 4, !tbaa !27
  %tmp11 = sext i32 %tmp10 to i64
  %tmp12 = tail call noalias i8* @malloc(i64 %tmp11) #9
  %tmp13 = icmp eq i8* %tmp12, null
  br i1 %tmp13, label %bb14, label %bb15

bb14:                                             ; preds = %bb8
  tail call void @__assert_fail(i8* getelementptr inbounds ([30 x i8], [30 x i8]* @.str.5, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.12, i64 0, i64 0), i32 37, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #10
  unreachable

bb15:                                             ; preds = %bb8
  %tmp16 = tail call i32 (i8*, i32, ...) @open(i8* %tmp9, i32 0) #9
  %tmp17 = icmp sgt i32 %tmp16, 0
  br i1 %tmp17, label %bb19, label %bb18

bb18:                                             ; preds = %bb15
  tail call void @__assert_fail(i8* getelementptr inbounds ([43 x i8], [43 x i8]* @.str.7, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.12, i64 0, i64 0), i32 39, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #10
  unreachable

bb19:                                             ; preds = %bb15
  tail call void @input_to_data(i32 %tmp16, i8* nonnull %tmp12) #9
  tail call void @run_benchmark(i8* nonnull %tmp12) #9
  tail call void @free(i8* nonnull %tmp12) #9
  %tmp20 = tail call i32 @puts(i8* getelementptr inbounds ([9 x i8], [9 x i8]* @str, i64 0, i64 0))
  ret i32 0
}

declare i32 @open(i8* nocapture readonly, i32, ...) local_unnamed_addr #6

; Function Attrs: nounwind
declare i32 @puts(i8* nocapture readonly) local_unnamed_addr #9

attributes #0 = { norecurse nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { argmemonly nounwind }
attributes #3 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { norecurse nounwind readonly uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #5 = { noreturn nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #6 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #7 = { argmemonly nounwind readonly "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #8 = { inlinehint nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #9 = { nounwind }
attributes #10 = { noreturn nounwind }
attributes #11 = { cold }
attributes #12 = { nounwind readonly }

!llvm.ident = !{!0, !0, !0, !0}
!llvm.module.flags = !{!1}

!0 = !{!"clang version 6.0.0 (git@github.com:seanzw/clang.git bb8d45f8ab88237f1fa0530b8ad9b96bf4a5e6cc) (git@github.com:seanzw/llvm.git 16ebb58ea40d384e8daa4c48d2bf7dd1ccfa5fcd)"}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{!3, !3, i64 0}
!3 = !{!"double", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C/C++ TBAA"}
!6 = !{!7}
!7 = distinct !{!7, !8}
!8 = distinct !{!8, !"LVerDomain"}
!9 = !{!10}
!10 = distinct !{!10, !8}
!11 = !{!12}
!12 = distinct !{!12, !8}
!13 = !{!7, !10}
!14 = distinct !{!14, !15}
!15 = !{!"llvm.loop.isvectorized", i32 1}
!16 = distinct !{!16, !15}
!17 = !{!18, !19, i64 48}
!18 = !{!"stat", !19, i64 0, !19, i64 8, !19, i64 16, !20, i64 24, !20, i64 28, !20, i64 32, !20, i64 36, !19, i64 40, !19, i64 48, !19, i64 56, !19, i64 64, !21, i64 72, !21, i64 88, !21, i64 104, !4, i64 120}
!19 = !{!"long", !4, i64 0}
!20 = !{!"int", !4, i64 0}
!21 = !{!"timespec", !19, i64 0, !19, i64 8}
!22 = !{!4, !4, i64 0}
!23 = !{!24, !24, i64 0}
!24 = !{!"any pointer", !4, i64 0}
!25 = !{!26, !26, i64 0}
!26 = !{!"short", !4, i64 0}
!27 = !{!20, !20, i64 0}
!28 = !{!19, !19, i64 0}
!29 = !{!30, !30, i64 0}
!30 = !{!"float", !4, i64 0}
