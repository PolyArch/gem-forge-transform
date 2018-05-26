; ModuleID = 'md.bc'
source_filename = "ld-temp.o"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct._IO_FILE = type { i32, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, %struct._IO_marker*, %struct._IO_FILE*, i32, i32, i64, i16, i8, [1 x i8], i8*, i64, i8*, i8*, i8*, i8*, i64, i32, [20 x i8] }
%struct._IO_marker = type { %struct._IO_marker*, %struct._IO_FILE*, i32 }
%struct.stat = type { i64, i64, i64, i32, i32, i32, i32, i64, i64, i64, i64, %struct.timespec, %struct.timespec, %struct.timespec, [3 x i64] }
%struct.timespec = type { i64, i64 }
%struct.__va_list_tag = type { i32, i32, i8*, i8* }

@INPUT_SIZE = internal dso_local global i32 28672, align 4
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
@.str.1.15 = private unnamed_addr constant [57 x i8] c"argc<4 && \22Usage: ./benchmark <input_file> <check_file>\22\00", align 1
@.str.2.16 = private unnamed_addr constant [23 x i8] c"../../common/harness.c\00", align 1
@__PRETTY_FUNCTION__.main = private unnamed_addr constant [23 x i8] c"int main(int, char **)\00", align 1
@.str.3 = private unnamed_addr constant [11 x i8] c"input.data\00", align 1
@.str.5 = private unnamed_addr constant [30 x i8] c"data!=NULL && \22Out of memory\22\00", align 1
@.str.7 = private unnamed_addr constant [43 x i8] c"in_fd>0 && \22Couldn't open input data file\22\00", align 1
@str = private unnamed_addr constant [9 x i8] c"Success.\00"

; Function Attrs: noinline norecurse nounwind uwtable
define internal dso_local void @md_kernel(double* nocapture %arg, double* nocapture %arg1, double* nocapture %arg2, double* nocapture readonly %arg3, double* nocapture readonly %arg4, double* nocapture readonly %arg5, i32* nocapture readonly %arg6) #0 {
bb:
  br label %bb7

bb7:                                              ; preds = %bb56, %bb
  %tmp = phi i64 [ 0, %bb ], [ %tmp62, %bb56 ]
  %tmp8 = getelementptr inbounds double, double* %arg3, i64 %tmp
  %tmp9 = load double, double* %tmp8, align 8, !tbaa !2
  %tmp10 = getelementptr inbounds double, double* %arg4, i64 %tmp
  %tmp11 = load double, double* %tmp10, align 8, !tbaa !2
  %tmp12 = getelementptr inbounds double, double* %arg5, i64 %tmp
  %tmp13 = load double, double* %tmp12, align 8, !tbaa !2
  %tmp14 = shl i64 %tmp, 4
  %tmp15 = insertelement <2 x double> undef, double %tmp9, i32 0
  %tmp16 = insertelement <2 x double> %tmp15, double %tmp13, i32 1
  br label %bb17

bb17:                                             ; preds = %bb17, %bb7
  %tmp18 = phi i64 [ 0, %bb7 ], [ %tmp54, %bb17 ]
  %tmp19 = phi double [ 0.000000e+00, %bb7 ], [ %tmp49, %bb17 ]
  %tmp20 = phi <2 x double> [ zeroinitializer, %bb7 ], [ %tmp53, %bb17 ]
  %tmp21 = add nuw nsw i64 %tmp18, %tmp14
  %tmp22 = getelementptr inbounds i32, i32* %arg6, i64 %tmp21
  %tmp23 = load i32, i32* %tmp22, align 4, !tbaa !6
  %tmp24 = sext i32 %tmp23 to i64
  %tmp25 = getelementptr inbounds double, double* %arg3, i64 %tmp24
  %tmp26 = load double, double* %tmp25, align 8, !tbaa !2
  %tmp27 = getelementptr inbounds double, double* %arg4, i64 %tmp24
  %tmp28 = load double, double* %tmp27, align 8, !tbaa !2
  %tmp29 = getelementptr inbounds double, double* %arg5, i64 %tmp24
  %tmp30 = load double, double* %tmp29, align 8, !tbaa !2
  %tmp31 = fsub double %tmp11, %tmp28
  %tmp32 = insertelement <2 x double> undef, double %tmp26, i32 0
  %tmp33 = insertelement <2 x double> %tmp32, double %tmp30, i32 1
  %tmp34 = fsub <2 x double> %tmp16, %tmp33
  %tmp35 = fmul double %tmp31, %tmp31
  %tmp36 = fmul <2 x double> %tmp34, %tmp34
  %tmp37 = extractelement <2 x double> %tmp36, i32 0
  %tmp38 = fadd double %tmp37, %tmp35
  %tmp39 = extractelement <2 x double> %tmp36, i32 1
  %tmp40 = fadd double %tmp38, %tmp39
  %tmp41 = fdiv double 1.000000e+00, %tmp40
  %tmp42 = fmul double %tmp41, %tmp41
  %tmp43 = fmul double %tmp41, %tmp42
  %tmp44 = fmul double %tmp43, 1.500000e+00
  %tmp45 = fadd double %tmp44, -2.000000e+00
  %tmp46 = fmul double %tmp43, %tmp45
  %tmp47 = fmul double %tmp41, %tmp46
  %tmp48 = fmul double %tmp31, %tmp47
  %tmp49 = fadd double %tmp19, %tmp48
  %tmp50 = insertelement <2 x double> undef, double %tmp47, i32 0
  %tmp51 = shufflevector <2 x double> %tmp50, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp52 = fmul <2 x double> %tmp34, %tmp51
  %tmp53 = fadd <2 x double> %tmp20, %tmp52
  %tmp54 = add nuw nsw i64 %tmp18, 1
  %tmp55 = icmp eq i64 %tmp54, 16
  br i1 %tmp55, label %bb56, label %bb17

bb56:                                             ; preds = %bb17
  %tmp57 = getelementptr inbounds double, double* %arg, i64 %tmp
  %tmp58 = extractelement <2 x double> %tmp53, i32 0
  store double %tmp58, double* %tmp57, align 8, !tbaa !2
  %tmp59 = getelementptr inbounds double, double* %arg1, i64 %tmp
  store double %tmp49, double* %tmp59, align 8, !tbaa !2
  %tmp60 = getelementptr inbounds double, double* %arg2, i64 %tmp
  %tmp61 = extractelement <2 x double> %tmp53, i32 1
  store double %tmp61, double* %tmp60, align 8, !tbaa !2
  %tmp62 = add nuw nsw i64 %tmp, 1
  %tmp63 = icmp eq i64 %tmp62, 256
  br i1 %tmp63, label %bb64, label %bb7

bb64:                                             ; preds = %bb56
  br label %bb65

bb65:                                             ; preds = %bb115, %bb64
  %tmp66 = phi i64 [ %tmp121, %bb115 ], [ 0, %bb64 ]
  %tmp67 = getelementptr inbounds double, double* %arg3, i64 %tmp66
  %tmp68 = load double, double* %tmp67, align 8, !tbaa !2
  %tmp69 = getelementptr inbounds double, double* %arg4, i64 %tmp66
  %tmp70 = load double, double* %tmp69, align 8, !tbaa !2
  %tmp71 = getelementptr inbounds double, double* %arg5, i64 %tmp66
  %tmp72 = load double, double* %tmp71, align 8, !tbaa !2
  %tmp73 = shl i64 %tmp66, 4
  %tmp74 = insertelement <2 x double> undef, double %tmp68, i32 0
  %tmp75 = insertelement <2 x double> %tmp74, double %tmp72, i32 1
  br label %bb76

bb76:                                             ; preds = %bb76, %bb65
  %tmp77 = phi i64 [ 0, %bb65 ], [ %tmp113, %bb76 ]
  %tmp78 = phi double [ 0.000000e+00, %bb65 ], [ %tmp108, %bb76 ]
  %tmp79 = phi <2 x double> [ zeroinitializer, %bb65 ], [ %tmp112, %bb76 ]
  %tmp80 = add nuw nsw i64 %tmp77, %tmp73
  %tmp81 = getelementptr inbounds i32, i32* %arg6, i64 %tmp80
  %tmp82 = load i32, i32* %tmp81, align 4, !tbaa !6
  %tmp83 = sext i32 %tmp82 to i64
  %tmp84 = getelementptr inbounds double, double* %arg3, i64 %tmp83
  %tmp85 = load double, double* %tmp84, align 8, !tbaa !2
  %tmp86 = getelementptr inbounds double, double* %arg4, i64 %tmp83
  %tmp87 = load double, double* %tmp86, align 8, !tbaa !2
  %tmp88 = getelementptr inbounds double, double* %arg5, i64 %tmp83
  %tmp89 = load double, double* %tmp88, align 8, !tbaa !2
  %tmp90 = fsub double %tmp70, %tmp87
  %tmp91 = insertelement <2 x double> undef, double %tmp85, i32 0
  %tmp92 = insertelement <2 x double> %tmp91, double %tmp89, i32 1
  %tmp93 = fsub <2 x double> %tmp75, %tmp92
  %tmp94 = fmul double %tmp90, %tmp90
  %tmp95 = fmul <2 x double> %tmp93, %tmp93
  %tmp96 = extractelement <2 x double> %tmp95, i32 0
  %tmp97 = fadd double %tmp96, %tmp94
  %tmp98 = extractelement <2 x double> %tmp95, i32 1
  %tmp99 = fadd double %tmp97, %tmp98
  %tmp100 = fdiv double 1.000000e+00, %tmp99
  %tmp101 = fmul double %tmp100, %tmp100
  %tmp102 = fmul double %tmp100, %tmp101
  %tmp103 = fmul double %tmp102, 1.500000e+00
  %tmp104 = fadd double %tmp103, -2.000000e+00
  %tmp105 = fmul double %tmp102, %tmp104
  %tmp106 = fmul double %tmp100, %tmp105
  %tmp107 = fmul double %tmp90, %tmp106
  %tmp108 = fadd double %tmp78, %tmp107
  %tmp109 = insertelement <2 x double> undef, double %tmp106, i32 0
  %tmp110 = shufflevector <2 x double> %tmp109, <2 x double> undef, <2 x i32> zeroinitializer
  %tmp111 = fmul <2 x double> %tmp93, %tmp110
  %tmp112 = fadd <2 x double> %tmp79, %tmp111
  %tmp113 = add nuw nsw i64 %tmp77, 1
  %tmp114 = icmp eq i64 %tmp113, 16
  br i1 %tmp114, label %bb115, label %bb76

bb115:                                            ; preds = %bb76
  %tmp116 = getelementptr inbounds double, double* %arg, i64 %tmp66
  %tmp117 = extractelement <2 x double> %tmp112, i32 0
  store double %tmp117, double* %tmp116, align 8, !tbaa !2
  %tmp118 = getelementptr inbounds double, double* %arg1, i64 %tmp66
  store double %tmp108, double* %tmp118, align 8, !tbaa !2
  %tmp119 = getelementptr inbounds double, double* %arg2, i64 %tmp66
  %tmp120 = extractelement <2 x double> %tmp112, i32 1
  store double %tmp120, double* %tmp119, align 8, !tbaa !2
  %tmp121 = add nuw nsw i64 %tmp66, 1
  %tmp122 = icmp eq i64 %tmp121, 256
  br i1 %tmp122, label %bb123, label %bb65

bb123:                                            ; preds = %bb115
  ret void
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @run_benchmark(i8* %arg) #1 {
bb:
  %tmp = bitcast i8* %arg to double*
  %tmp1 = getelementptr inbounds i8, i8* %arg, i64 2048
  %tmp2 = bitcast i8* %tmp1 to double*
  %tmp3 = getelementptr inbounds i8, i8* %arg, i64 4096
  %tmp4 = bitcast i8* %tmp3 to double*
  %tmp5 = getelementptr inbounds i8, i8* %arg, i64 6144
  %tmp6 = bitcast i8* %tmp5 to double*
  %tmp7 = getelementptr inbounds i8, i8* %arg, i64 8192
  %tmp8 = bitcast i8* %tmp7 to double*
  %tmp9 = getelementptr inbounds i8, i8* %arg, i64 10240
  %tmp10 = bitcast i8* %tmp9 to double*
  %tmp11 = getelementptr inbounds i8, i8* %arg, i64 12288
  %tmp12 = bitcast i8* %tmp11 to i32*
  tail call void @md_kernel(double* %tmp, double* nonnull %tmp2, double* nonnull %tmp4, double* nonnull %tmp6, double* nonnull %tmp8, double* nonnull %tmp10, i32* nonnull %tmp12) #8
  ret void
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @input_to_data(i32 %arg, i8* %arg1) #1 {
bb:
  tail call void @llvm.memset.p0i8.i64(i8* %arg1, i8 0, i64 28672, i32 1, i1 false)
  %tmp = tail call i8* @readfile(i32 %arg) #8
  %tmp2 = tail call i8* @find_section_start(i8* %tmp, i32 1) #8
  %tmp3 = getelementptr inbounds i8, i8* %arg1, i64 6144
  %tmp4 = bitcast i8* %tmp3 to double*
  %tmp5 = tail call i32 @parse_double_array(i8* %tmp2, double* nonnull %tmp4, i32 256) #8
  %tmp6 = tail call i8* @find_section_start(i8* %tmp, i32 2) #8
  %tmp7 = getelementptr inbounds i8, i8* %arg1, i64 8192
  %tmp8 = bitcast i8* %tmp7 to double*
  %tmp9 = tail call i32 @parse_double_array(i8* %tmp6, double* nonnull %tmp8, i32 256) #8
  %tmp10 = tail call i8* @find_section_start(i8* %tmp, i32 3) #8
  %tmp11 = getelementptr inbounds i8, i8* %arg1, i64 10240
  %tmp12 = bitcast i8* %tmp11 to double*
  %tmp13 = tail call i32 @parse_double_array(i8* %tmp10, double* nonnull %tmp12, i32 256) #8
  %tmp14 = tail call i8* @find_section_start(i8* %tmp, i32 4) #8
  %tmp15 = getelementptr inbounds i8, i8* %arg1, i64 12288
  %tmp16 = bitcast i8* %tmp15 to i32*
  %tmp17 = tail call i32 @parse_int32_t_array(i8* %tmp14, i32* nonnull %tmp16, i32 4096) #8
  tail call void @free(i8* %tmp) #8
  ret void
}

; Function Attrs: argmemonly nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i32, i1) #2

; Function Attrs: nounwind
declare void @free(i8* nocapture) local_unnamed_addr #3

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @data_to_input(i32 %arg, i8* %arg1) #1 {
bb:
  %tmp = tail call i32 @write_section_header(i32 %arg) #8
  %tmp2 = getelementptr inbounds i8, i8* %arg1, i64 6144
  %tmp3 = bitcast i8* %tmp2 to double*
  %tmp4 = tail call i32 @write_double_array(i32 %arg, double* nonnull %tmp3, i32 256) #8
  %tmp5 = tail call i32 @write_section_header(i32 %arg) #8
  %tmp6 = getelementptr inbounds i8, i8* %arg1, i64 8192
  %tmp7 = bitcast i8* %tmp6 to double*
  %tmp8 = tail call i32 @write_double_array(i32 %arg, double* nonnull %tmp7, i32 256) #8
  %tmp9 = tail call i32 @write_section_header(i32 %arg) #8
  %tmp10 = getelementptr inbounds i8, i8* %arg1, i64 10240
  %tmp11 = bitcast i8* %tmp10 to double*
  %tmp12 = tail call i32 @write_double_array(i32 %arg, double* nonnull %tmp11, i32 256) #8
  %tmp13 = tail call i32 @write_section_header(i32 %arg) #8
  %tmp14 = getelementptr inbounds i8, i8* %arg1, i64 12288
  %tmp15 = bitcast i8* %tmp14 to i32*
  %tmp16 = tail call i32 @write_int32_t_array(i32 %arg, i32* nonnull %tmp15, i32 4096) #8
  ret void
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @output_to_data(i32 %arg, i8* %arg1) #1 {
bb:
  tail call void @llvm.memset.p0i8.i64(i8* %arg1, i8 0, i64 28672, i32 1, i1 false)
  %tmp = tail call i8* @readfile(i32 %arg) #8
  %tmp2 = tail call i8* @find_section_start(i8* %tmp, i32 1) #8
  %tmp3 = bitcast i8* %arg1 to double*
  %tmp4 = tail call i32 @parse_double_array(i8* %tmp2, double* %tmp3, i32 256) #8
  %tmp5 = tail call i8* @find_section_start(i8* %tmp, i32 2) #8
  %tmp6 = getelementptr inbounds i8, i8* %arg1, i64 2048
  %tmp7 = bitcast i8* %tmp6 to double*
  %tmp8 = tail call i32 @parse_double_array(i8* %tmp5, double* nonnull %tmp7, i32 256) #8
  %tmp9 = tail call i8* @find_section_start(i8* %tmp, i32 3) #8
  %tmp10 = getelementptr inbounds i8, i8* %arg1, i64 4096
  %tmp11 = bitcast i8* %tmp10 to double*
  %tmp12 = tail call i32 @parse_double_array(i8* %tmp9, double* nonnull %tmp11, i32 256) #8
  tail call void @free(i8* %tmp) #8
  ret void
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @data_to_output(i32 %arg, i8* %arg1) #1 {
bb:
  %tmp = tail call i32 @write_section_header(i32 %arg) #8
  %tmp2 = bitcast i8* %arg1 to double*
  %tmp3 = tail call i32 @write_double_array(i32 %arg, double* %tmp2, i32 256) #8
  %tmp4 = tail call i32 @write_section_header(i32 %arg) #8
  %tmp5 = getelementptr inbounds i8, i8* %arg1, i64 2048
  %tmp6 = bitcast i8* %tmp5 to double*
  %tmp7 = tail call i32 @write_double_array(i32 %arg, double* nonnull %tmp6, i32 256) #8
  %tmp8 = tail call i32 @write_section_header(i32 %arg) #8
  %tmp9 = getelementptr inbounds i8, i8* %arg1, i64 4096
  %tmp10 = bitcast i8* %tmp9 to double*
  %tmp11 = tail call i32 @write_double_array(i32 %arg, double* nonnull %tmp10, i32 256) #8
  ret void
}

; Function Attrs: noinline norecurse nounwind readonly uwtable
define internal dso_local i32 @check_data(i8* nocapture readonly %arg, i8* nocapture readonly %arg1) #4 {
bb:
  %tmp = bitcast i8* %arg to [256 x double]*
  %tmp2 = bitcast i8* %arg1 to [256 x double]*
  %tmp3 = getelementptr inbounds i8, i8* %arg, i64 2048
  %tmp4 = bitcast i8* %tmp3 to [256 x double]*
  %tmp5 = getelementptr inbounds i8, i8* %arg1, i64 2048
  %tmp6 = bitcast i8* %tmp5 to [256 x double]*
  %tmp7 = getelementptr inbounds i8, i8* %arg, i64 4096
  %tmp8 = bitcast i8* %tmp7 to [256 x double]*
  %tmp9 = getelementptr inbounds i8, i8* %arg1, i64 4096
  %tmp10 = bitcast i8* %tmp9 to [256 x double]*
  br label %bb11

bb11:                                             ; preds = %bb11, %bb
  %tmp12 = phi i64 [ 0, %bb ], [ %tmp87, %bb11 ]
  %tmp13 = phi <2 x i32> [ zeroinitializer, %bb ], [ %tmp85, %bb11 ]
  %tmp14 = phi <2 x i32> [ zeroinitializer, %bb ], [ %tmp86, %bb11 ]
  %tmp15 = getelementptr inbounds [256 x double], [256 x double]* %tmp, i64 0, i64 %tmp12
  %tmp16 = bitcast double* %tmp15 to <2 x double>*
  %tmp17 = load <2 x double>, <2 x double>* %tmp16, align 8, !tbaa !2
  %tmp18 = getelementptr double, double* %tmp15, i64 2
  %tmp19 = bitcast double* %tmp18 to <2 x double>*
  %tmp20 = load <2 x double>, <2 x double>* %tmp19, align 8, !tbaa !2
  %tmp21 = getelementptr inbounds [256 x double], [256 x double]* %tmp2, i64 0, i64 %tmp12
  %tmp22 = bitcast double* %tmp21 to <2 x double>*
  %tmp23 = load <2 x double>, <2 x double>* %tmp22, align 8, !tbaa !2
  %tmp24 = getelementptr double, double* %tmp21, i64 2
  %tmp25 = bitcast double* %tmp24 to <2 x double>*
  %tmp26 = load <2 x double>, <2 x double>* %tmp25, align 8, !tbaa !2
  %tmp27 = fsub <2 x double> %tmp17, %tmp23
  %tmp28 = fsub <2 x double> %tmp20, %tmp26
  %tmp29 = getelementptr inbounds [256 x double], [256 x double]* %tmp4, i64 0, i64 %tmp12
  %tmp30 = bitcast double* %tmp29 to <2 x double>*
  %tmp31 = load <2 x double>, <2 x double>* %tmp30, align 8, !tbaa !2
  %tmp32 = getelementptr double, double* %tmp29, i64 2
  %tmp33 = bitcast double* %tmp32 to <2 x double>*
  %tmp34 = load <2 x double>, <2 x double>* %tmp33, align 8, !tbaa !2
  %tmp35 = getelementptr inbounds [256 x double], [256 x double]* %tmp6, i64 0, i64 %tmp12
  %tmp36 = bitcast double* %tmp35 to <2 x double>*
  %tmp37 = load <2 x double>, <2 x double>* %tmp36, align 8, !tbaa !2
  %tmp38 = getelementptr double, double* %tmp35, i64 2
  %tmp39 = bitcast double* %tmp38 to <2 x double>*
  %tmp40 = load <2 x double>, <2 x double>* %tmp39, align 8, !tbaa !2
  %tmp41 = fsub <2 x double> %tmp31, %tmp37
  %tmp42 = fsub <2 x double> %tmp34, %tmp40
  %tmp43 = getelementptr inbounds [256 x double], [256 x double]* %tmp8, i64 0, i64 %tmp12
  %tmp44 = bitcast double* %tmp43 to <2 x double>*
  %tmp45 = load <2 x double>, <2 x double>* %tmp44, align 8, !tbaa !2
  %tmp46 = getelementptr double, double* %tmp43, i64 2
  %tmp47 = bitcast double* %tmp46 to <2 x double>*
  %tmp48 = load <2 x double>, <2 x double>* %tmp47, align 8, !tbaa !2
  %tmp49 = getelementptr inbounds [256 x double], [256 x double]* %tmp10, i64 0, i64 %tmp12
  %tmp50 = bitcast double* %tmp49 to <2 x double>*
  %tmp51 = load <2 x double>, <2 x double>* %tmp50, align 8, !tbaa !2
  %tmp52 = getelementptr double, double* %tmp49, i64 2
  %tmp53 = bitcast double* %tmp52 to <2 x double>*
  %tmp54 = load <2 x double>, <2 x double>* %tmp53, align 8, !tbaa !2
  %tmp55 = fsub <2 x double> %tmp45, %tmp51
  %tmp56 = fsub <2 x double> %tmp48, %tmp54
  %tmp57 = fcmp olt <2 x double> %tmp27, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp58 = fcmp olt <2 x double> %tmp28, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp59 = fcmp ogt <2 x double> %tmp27, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp60 = fcmp ogt <2 x double> %tmp28, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp61 = or <2 x i1> %tmp57, %tmp59
  %tmp62 = or <2 x i1> %tmp58, %tmp60
  %tmp63 = zext <2 x i1> %tmp61 to <2 x i32>
  %tmp64 = zext <2 x i1> %tmp62 to <2 x i32>
  %tmp65 = or <2 x i32> %tmp13, %tmp63
  %tmp66 = or <2 x i32> %tmp14, %tmp64
  %tmp67 = fcmp olt <2 x double> %tmp41, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp68 = fcmp olt <2 x double> %tmp42, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp69 = fcmp ogt <2 x double> %tmp41, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp70 = fcmp ogt <2 x double> %tmp42, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp71 = or <2 x i1> %tmp67, %tmp69
  %tmp72 = or <2 x i1> %tmp68, %tmp70
  %tmp73 = zext <2 x i1> %tmp71 to <2 x i32>
  %tmp74 = zext <2 x i1> %tmp72 to <2 x i32>
  %tmp75 = or <2 x i32> %tmp65, %tmp73
  %tmp76 = or <2 x i32> %tmp66, %tmp74
  %tmp77 = fcmp olt <2 x double> %tmp55, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp78 = fcmp olt <2 x double> %tmp56, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp79 = fcmp ogt <2 x double> %tmp55, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp80 = fcmp ogt <2 x double> %tmp56, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp81 = or <2 x i1> %tmp77, %tmp79
  %tmp82 = or <2 x i1> %tmp78, %tmp80
  %tmp83 = zext <2 x i1> %tmp81 to <2 x i32>
  %tmp84 = zext <2 x i1> %tmp82 to <2 x i32>
  %tmp85 = or <2 x i32> %tmp75, %tmp83
  %tmp86 = or <2 x i32> %tmp76, %tmp84
  %tmp87 = add i64 %tmp12, 4
  %tmp88 = icmp eq i64 %tmp87, 256
  br i1 %tmp88, label %bb89, label %bb11, !llvm.loop !8

bb89:                                             ; preds = %bb11
  %tmp90 = or <2 x i32> %tmp86, %tmp85
  %tmp91 = shufflevector <2 x i32> %tmp90, <2 x i32> undef, <2 x i32> <i32 1, i32 undef>
  %tmp92 = or <2 x i32> %tmp90, %tmp91
  %tmp93 = extractelement <2 x i32> %tmp92, i32 0
  %tmp94 = icmp eq i32 %tmp93, 0
  %tmp95 = zext i1 %tmp94 to i32
  ret i32 %tmp95
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
  %tmp10 = load i64, i64* %tmp9, align 8, !tbaa !10
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
  store i8 0, i8* %tmp27, align 1, !tbaa !14
  %tmp28 = call i32 @close(i32 %arg) #8
  call void @llvm.lifetime.end.p0i8(i64 144, i8* nonnull %tmp1) #8
  ret i8* %tmp15
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64, i8* nocapture) #2

; Function Attrs: noreturn nounwind
declare void @__assert_fail(i8*, i8*, i32, i8*) local_unnamed_addr #5

; Function Attrs: noinline nounwind uwtable
define available_externally dso_local i32 @fstat(i32 %arg, %struct.stat* nonnull %arg1) local_unnamed_addr #1 {
bb:
  %tmp = tail call i32 @__fxstat(i32 1, i32 %arg, %struct.stat* nonnull %arg1) #8
  ret i32 %tmp
}

; Function Attrs: nounwind
declare noalias i8* @malloc(i64) local_unnamed_addr #3

declare i64 @read(i32, i8* nocapture, i64) local_unnamed_addr #6

declare i32 @close(i32) local_unnamed_addr #6

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64, i8* nocapture) #2

; Function Attrs: nounwind
declare i32 @__fxstat(i32, i32, %struct.stat*) local_unnamed_addr #3

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
  %tmp6 = load i8, i8* %arg, align 1, !tbaa !14
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
  %tmp13 = load i8, i8* %tmp12, align 1, !tbaa !14
  br label %bb24

bb14:                                             ; preds = %bb7
  %tmp15 = getelementptr inbounds i8, i8* %tmp10, i64 1
  %tmp16 = load i8, i8* %tmp15, align 1, !tbaa !14
  %tmp17 = icmp eq i8 %tmp16, 37
  br i1 %tmp17, label %bb18, label %bb24

bb18:                                             ; preds = %bb14
  %tmp19 = getelementptr inbounds i8, i8* %tmp10, i64 2
  %tmp20 = load i8, i8* %tmp19, align 1, !tbaa !14
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
  %tmp9 = load i8, i8* %arg, align 1, !tbaa !14
  %tmp10 = icmp eq i8 %tmp9, 0
  br i1 %tmp10, label %bb43, label %bb11

bb11:                                             ; preds = %bb8
  %tmp12 = getelementptr inbounds i8, i8* %arg, i64 1
  %tmp13 = load i8, i8* %tmp12, align 1, !tbaa !14
  br label %bb17

bb14:                                             ; preds = %bb31
  %tmp15 = load i8, i8* %tmp24, align 1, !tbaa !14
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
  %tmp29 = load i8, i8* %tmp28, align 1, !tbaa !14
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
  store i8 0, i8* %tmp46, align 1, !tbaa !14
  br label %bb47

bb47:                                             ; preds = %bb43, %bb6
  ret i32 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture writeonly, i8* nocapture readonly, i64, i32, i1) #2

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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !15
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = trunc i64 %tmp16 to i8
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !15
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !14
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !15
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #10
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i8, i8* %arg1, i64 %tmp14
  store i8 %tmp17, i8* %tmp26, align 1, !tbaa !14
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !14
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
  store i8 10, i8* %tmp39, align 1, !tbaa !14
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #8
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !15
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = trunc i64 %tmp16 to i16
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !15
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !14
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !15
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #10
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i16, i16* %arg1, i64 %tmp14
  store i16 %tmp17, i16* %tmp26, align 2, !tbaa !17
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !14
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
  store i8 10, i8* %tmp39, align 1, !tbaa !14
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !15
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = trunc i64 %tmp16 to i32
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !15
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !14
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !15
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #10
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i32, i32* %arg1, i64 %tmp14
  store i32 %tmp17, i32* %tmp26, align 4, !tbaa !6
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !14
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
  store i8 10, i8* %tmp39, align 1, !tbaa !14
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !15
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !15
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !14
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !15
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #10
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds i64, i64* %arg1, i64 %tmp14
  store i64 %tmp16, i64* %tmp25, align 8, !tbaa !19
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !14
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
  store i8 10, i8* %tmp38, align 1, !tbaa !14
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !15
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = trunc i64 %tmp16 to i8
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !15
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !14
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !15
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #10
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i8, i8* %arg1, i64 %tmp14
  store i8 %tmp17, i8* %tmp26, align 1, !tbaa !14
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !14
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
  store i8 10, i8* %tmp39, align 1, !tbaa !14
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !15
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = trunc i64 %tmp16 to i16
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !15
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !14
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !15
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #10
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i16, i16* %arg1, i64 %tmp14
  store i16 %tmp17, i16* %tmp26, align 2, !tbaa !17
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !14
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
  store i8 10, i8* %tmp39, align 1, !tbaa !14
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !15
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = trunc i64 %tmp16 to i32
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !15
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !14
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !15
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #10
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i32, i32* %arg1, i64 %tmp14
  store i32 %tmp17, i32* %tmp26, align 4, !tbaa !6
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !14
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
  store i8 10, i8* %tmp39, align 1, !tbaa !14
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !15
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #8
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !15
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !14
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !15
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #10
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds i64, i64* %arg1, i64 %tmp14
  store i64 %tmp16, i64* %tmp25, align 8, !tbaa !19
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !14
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
  store i8 10, i8* %tmp38, align 1, !tbaa !14
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !15
  %tmp16 = call float @strtof(i8* nonnull %tmp15, i8** nonnull %tmp) #8
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !15
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !14
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !15
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #10
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds float, float* %arg1, i64 %tmp14
  store float %tmp16, float* %tmp25, align 4, !tbaa !20
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !14
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
  store i8 10, i8* %tmp38, align 1, !tbaa !14
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #8
  ret i32 0
}

; Function Attrs: nounwind
declare float @strtof(i8* readonly, i8** nocapture) local_unnamed_addr #3

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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !15
  %tmp16 = call double @strtod(i8* nonnull %tmp15, i8** nonnull %tmp) #8
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !15
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !14
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !15
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #10
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds double, double* %arg1, i64 %tmp14
  store double %tmp16, double* %tmp25, align 8, !tbaa !2
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #11
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !14
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
  store i8 10, i8* %tmp38, align 1, !tbaa !14
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #8
  ret i32 0
}

; Function Attrs: nounwind
declare double @strtod(i8* readonly, i8** nocapture) local_unnamed_addr #3

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
  %tmp11 = load i8, i8* %tmp10, align 1, !tbaa !14
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
declare i32 @vsnprintf(i8* nocapture, i64, i8* nocapture readonly, %struct.__va_list_tag*) local_unnamed_addr #3

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
  %tmp11 = load i16, i16* %tmp10, align 2, !tbaa !17
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
  %tmp11 = load i32, i32* %tmp10, align 4, !tbaa !6
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
  %tmp11 = load i64, i64* %tmp10, align 8, !tbaa !19
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
  %tmp11 = load i8, i8* %tmp10, align 1, !tbaa !14
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
  %tmp11 = load i16, i16* %tmp10, align 2, !tbaa !17
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
  %tmp11 = load i32, i32* %tmp10, align 4, !tbaa !6
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
  %tmp11 = load i64, i64* %tmp10, align 8, !tbaa !19
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
  %tmp11 = load float, float* %tmp10, align 4, !tbaa !20
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
  %tmp11 = load double, double* %tmp10, align 8, !tbaa !2
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
  tail call void @__assert_fail(i8* getelementptr inbounds ([57 x i8], [57 x i8]* @.str.1.15, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.16, i64 0, i64 0), i32 21, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #9
  unreachable

bb3:                                              ; preds = %bb
  %tmp4 = icmp sgt i32 %arg, 1
  br i1 %tmp4, label %bb5, label %bb8

bb5:                                              ; preds = %bb3
  %tmp6 = getelementptr inbounds i8*, i8** %arg1, i64 1
  %tmp7 = load i8*, i8** %tmp6, align 8, !tbaa !15
  br label %bb8

bb8:                                              ; preds = %bb5, %bb3
  %tmp9 = phi i8* [ %tmp7, %bb5 ], [ getelementptr inbounds ([11 x i8], [11 x i8]* @.str.3, i64 0, i64 0), %bb3 ]
  %tmp10 = load i32, i32* @INPUT_SIZE, align 4, !tbaa !6
  %tmp11 = sext i32 %tmp10 to i64
  %tmp12 = tail call noalias i8* @malloc(i64 %tmp11) #8
  %tmp13 = icmp eq i8* %tmp12, null
  br i1 %tmp13, label %bb14, label %bb15

bb14:                                             ; preds = %bb8
  tail call void @__assert_fail(i8* getelementptr inbounds ([30 x i8], [30 x i8]* @.str.5, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.16, i64 0, i64 0), i32 37, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #9
  unreachable

bb15:                                             ; preds = %bb8
  %tmp16 = tail call i32 (i8*, i32, ...) @open(i8* %tmp9, i32 0) #8
  %tmp17 = icmp sgt i32 %tmp16, 0
  br i1 %tmp17, label %bb19, label %bb18

bb18:                                             ; preds = %bb15
  tail call void @__assert_fail(i8* getelementptr inbounds ([43 x i8], [43 x i8]* @.str.7, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.16, i64 0, i64 0), i32 39, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #9
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
attributes #2 = { argmemonly nounwind }
attributes #3 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { noinline norecurse nounwind readonly uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
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
!3 = !{!"double", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C/C++ TBAA"}
!6 = !{!7, !7, i64 0}
!7 = !{!"int", !4, i64 0}
!8 = distinct !{!8, !9}
!9 = !{!"llvm.loop.isvectorized", i32 1}
!10 = !{!11, !12, i64 48}
!11 = !{!"stat", !12, i64 0, !12, i64 8, !12, i64 16, !7, i64 24, !7, i64 28, !7, i64 32, !7, i64 36, !12, i64 40, !12, i64 48, !12, i64 56, !12, i64 64, !13, i64 72, !13, i64 88, !13, i64 104, !4, i64 120}
!12 = !{!"long", !4, i64 0}
!13 = !{!"timespec", !12, i64 0, !12, i64 8}
!14 = !{!4, !4, i64 0}
!15 = !{!16, !16, i64 0}
!16 = !{!"any pointer", !4, i64 0}
!17 = !{!18, !18, i64 0}
!18 = !{!"short", !4, i64 0}
!19 = !{!12, !12, i64 0}
!20 = !{!21, !21, i64 0}
!21 = !{!"float", !4, i64 0}
