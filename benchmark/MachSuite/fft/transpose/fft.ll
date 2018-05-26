; ModuleID = 'fft.bc'
source_filename = "ld-temp.o"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct._IO_FILE = type { i32, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, %struct._IO_marker*, %struct._IO_FILE*, i32, i32, i64, i16, i8, [1 x i8], i8*, i64, i8*, i8*, i8*, i8*, i64, i32, [20 x i8] }
%struct._IO_marker = type { %struct._IO_marker*, %struct._IO_FILE*, i32 }
%struct.stat = type { i64, i64, i64, i32, i32, i32, i32, i64, i64, i64, i64, %struct.timespec, %struct.timespec, %struct.timespec, [3 x i64] }
%struct.timespec = type { i64, i64 }
%struct.__va_list_tag = type { i32, i32, i8*, i8* }

@INPUT_SIZE = internal dso_local global i32 8192, align 4
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

; Function Attrs: nounwind uwtable
define internal dso_local void @twiddles8(double* nocapture %arg, double* nocapture %arg1, i32 %arg2, i32 %arg3) #0 {
bb:
  %tmp = sitofp i32 %arg3 to double
  %tmp4 = sitofp i32 %arg2 to double
  %tmp5 = fdiv double 0xC03921FB54411744, %tmp
  %tmp6 = fmul double %tmp5, %tmp4
  %tmp7 = tail call double @cos(double %tmp6) #10
  %tmp8 = tail call double @sin(double %tmp6) #10
  %tmp9 = getelementptr inbounds double, double* %arg, i64 1
  %tmp10 = load double, double* %tmp9, align 8, !tbaa !2
  %tmp11 = fmul double %tmp7, %tmp10
  %tmp12 = getelementptr inbounds double, double* %arg1, i64 1
  %tmp13 = load double, double* %tmp12, align 8, !tbaa !2
  %tmp14 = fmul double %tmp8, %tmp13
  %tmp15 = fsub double %tmp11, %tmp14
  store double %tmp15, double* %tmp9, align 8, !tbaa !2
  %tmp16 = fmul double %tmp8, %tmp10
  %tmp17 = load double, double* %tmp12, align 8, !tbaa !2
  %tmp18 = fmul double %tmp7, %tmp17
  %tmp19 = fadd double %tmp16, %tmp18
  store double %tmp19, double* %tmp12, align 8, !tbaa !2
  %tmp20 = fdiv double 0xC02921FB54411744, %tmp
  %tmp21 = fmul double %tmp20, %tmp4
  %tmp22 = tail call double @cos(double %tmp21) #10
  %tmp23 = tail call double @sin(double %tmp21) #10
  %tmp24 = getelementptr inbounds double, double* %arg, i64 2
  %tmp25 = load double, double* %tmp24, align 8, !tbaa !2
  %tmp26 = fmul double %tmp22, %tmp25
  %tmp27 = getelementptr inbounds double, double* %arg1, i64 2
  %tmp28 = load double, double* %tmp27, align 8, !tbaa !2
  %tmp29 = fmul double %tmp23, %tmp28
  %tmp30 = fsub double %tmp26, %tmp29
  store double %tmp30, double* %tmp24, align 8, !tbaa !2
  %tmp31 = fmul double %tmp23, %tmp25
  %tmp32 = load double, double* %tmp27, align 8, !tbaa !2
  %tmp33 = fmul double %tmp22, %tmp32
  %tmp34 = fadd double %tmp31, %tmp33
  store double %tmp34, double* %tmp27, align 8, !tbaa !2
  %tmp35 = fdiv double 0xC042D97C7F30D173, %tmp
  %tmp36 = fmul double %tmp35, %tmp4
  %tmp37 = tail call double @cos(double %tmp36) #10
  %tmp38 = tail call double @sin(double %tmp36) #10
  %tmp39 = getelementptr inbounds double, double* %arg, i64 3
  %tmp40 = load double, double* %tmp39, align 8, !tbaa !2
  %tmp41 = fmul double %tmp37, %tmp40
  %tmp42 = getelementptr inbounds double, double* %arg1, i64 3
  %tmp43 = load double, double* %tmp42, align 8, !tbaa !2
  %tmp44 = fmul double %tmp38, %tmp43
  %tmp45 = fsub double %tmp41, %tmp44
  store double %tmp45, double* %tmp39, align 8, !tbaa !2
  %tmp46 = fmul double %tmp38, %tmp40
  %tmp47 = load double, double* %tmp42, align 8, !tbaa !2
  %tmp48 = fmul double %tmp37, %tmp47
  %tmp49 = fadd double %tmp46, %tmp48
  store double %tmp49, double* %tmp42, align 8, !tbaa !2
  %tmp50 = fdiv double 0xC01921FB54411744, %tmp
  %tmp51 = fmul double %tmp50, %tmp4
  %tmp52 = tail call double @cos(double %tmp51) #10
  %tmp53 = tail call double @sin(double %tmp51) #10
  %tmp54 = getelementptr inbounds double, double* %arg, i64 4
  %tmp55 = load double, double* %tmp54, align 8, !tbaa !2
  %tmp56 = fmul double %tmp52, %tmp55
  %tmp57 = getelementptr inbounds double, double* %arg1, i64 4
  %tmp58 = load double, double* %tmp57, align 8, !tbaa !2
  %tmp59 = fmul double %tmp53, %tmp58
  %tmp60 = fsub double %tmp56, %tmp59
  store double %tmp60, double* %tmp54, align 8, !tbaa !2
  %tmp61 = fmul double %tmp53, %tmp55
  %tmp62 = load double, double* %tmp57, align 8, !tbaa !2
  %tmp63 = fmul double %tmp52, %tmp62
  %tmp64 = fadd double %tmp61, %tmp63
  store double %tmp64, double* %tmp57, align 8, !tbaa !2
  %tmp65 = fdiv double 0xC03F6A7A29515D15, %tmp
  %tmp66 = fmul double %tmp65, %tmp4
  %tmp67 = tail call double @cos(double %tmp66) #10
  %tmp68 = tail call double @sin(double %tmp66) #10
  %tmp69 = getelementptr inbounds double, double* %arg, i64 5
  %tmp70 = load double, double* %tmp69, align 8, !tbaa !2
  %tmp71 = fmul double %tmp67, %tmp70
  %tmp72 = getelementptr inbounds double, double* %arg1, i64 5
  %tmp73 = load double, double* %tmp72, align 8, !tbaa !2
  %tmp74 = fmul double %tmp68, %tmp73
  %tmp75 = fsub double %tmp71, %tmp74
  store double %tmp75, double* %tmp69, align 8, !tbaa !2
  %tmp76 = fmul double %tmp68, %tmp70
  %tmp77 = load double, double* %tmp72, align 8, !tbaa !2
  %tmp78 = fmul double %tmp67, %tmp77
  %tmp79 = fadd double %tmp76, %tmp78
  store double %tmp79, double* %tmp72, align 8, !tbaa !2
  %tmp80 = fdiv double 0xC032D97C7F30D173, %tmp
  %tmp81 = fmul double %tmp80, %tmp4
  %tmp82 = tail call double @cos(double %tmp81) #10
  %tmp83 = tail call double @sin(double %tmp81) #10
  %tmp84 = getelementptr inbounds double, double* %arg, i64 6
  %tmp85 = load double, double* %tmp84, align 8, !tbaa !2
  %tmp86 = fmul double %tmp82, %tmp85
  %tmp87 = getelementptr inbounds double, double* %arg1, i64 6
  %tmp88 = load double, double* %tmp87, align 8, !tbaa !2
  %tmp89 = fmul double %tmp83, %tmp88
  %tmp90 = fsub double %tmp86, %tmp89
  store double %tmp90, double* %tmp84, align 8, !tbaa !2
  %tmp91 = fmul double %tmp83, %tmp85
  %tmp92 = load double, double* %tmp87, align 8, !tbaa !2
  %tmp93 = fmul double %tmp82, %tmp92
  %tmp94 = fadd double %tmp91, %tmp93
  store double %tmp94, double* %tmp87, align 8, !tbaa !2
  %tmp95 = fdiv double 0xC045FDBBE9B8F45C, %tmp
  %tmp96 = fmul double %tmp95, %tmp4
  %tmp97 = tail call double @cos(double %tmp96) #10
  %tmp98 = tail call double @sin(double %tmp96) #10
  %tmp99 = getelementptr inbounds double, double* %arg, i64 7
  %tmp100 = load double, double* %tmp99, align 8, !tbaa !2
  %tmp101 = fmul double %tmp97, %tmp100
  %tmp102 = getelementptr inbounds double, double* %arg1, i64 7
  %tmp103 = load double, double* %tmp102, align 8, !tbaa !2
  %tmp104 = fmul double %tmp98, %tmp103
  %tmp105 = fsub double %tmp101, %tmp104
  store double %tmp105, double* %tmp99, align 8, !tbaa !2
  %tmp106 = fmul double %tmp98, %tmp100
  %tmp107 = load double, double* %tmp102, align 8, !tbaa !2
  %tmp108 = fmul double %tmp97, %tmp107
  %tmp109 = fadd double %tmp106, %tmp108
  store double %tmp109, double* %tmp102, align 8, !tbaa !2
  ret void
}

; Function Attrs: nounwind
declare double @cos(double) local_unnamed_addr #1

; Function Attrs: nounwind
declare double @sin(double) local_unnamed_addr #1

; Function Attrs: norecurse nounwind uwtable
define internal dso_local void @loadx8(double* nocapture %arg, double* nocapture readonly %arg1, i32 %arg2, i32 %arg3) #2 {
bb:
  %tmp = sext i32 %arg2 to i64
  %tmp4 = getelementptr inbounds double, double* %arg1, i64 %tmp
  %tmp5 = bitcast double* %tmp4 to i64*
  %tmp6 = load i64, i64* %tmp5, align 8, !tbaa !2
  %tmp7 = bitcast double* %arg to i64*
  store i64 %tmp6, i64* %tmp7, align 8, !tbaa !2
  %tmp8 = add nsw i32 %arg3, %arg2
  %tmp9 = sext i32 %tmp8 to i64
  %tmp10 = getelementptr inbounds double, double* %arg1, i64 %tmp9
  %tmp11 = bitcast double* %tmp10 to i64*
  %tmp12 = load i64, i64* %tmp11, align 8, !tbaa !2
  %tmp13 = getelementptr inbounds double, double* %arg, i64 1
  %tmp14 = bitcast double* %tmp13 to i64*
  store i64 %tmp12, i64* %tmp14, align 8, !tbaa !2
  %tmp15 = shl i32 %arg3, 1
  %tmp16 = add nsw i32 %tmp15, %arg2
  %tmp17 = sext i32 %tmp16 to i64
  %tmp18 = getelementptr inbounds double, double* %arg1, i64 %tmp17
  %tmp19 = bitcast double* %tmp18 to i64*
  %tmp20 = load i64, i64* %tmp19, align 8, !tbaa !2
  %tmp21 = getelementptr inbounds double, double* %arg, i64 2
  %tmp22 = bitcast double* %tmp21 to i64*
  store i64 %tmp20, i64* %tmp22, align 8, !tbaa !2
  %tmp23 = mul nsw i32 %arg3, 3
  %tmp24 = add nsw i32 %tmp23, %arg2
  %tmp25 = sext i32 %tmp24 to i64
  %tmp26 = getelementptr inbounds double, double* %arg1, i64 %tmp25
  %tmp27 = bitcast double* %tmp26 to i64*
  %tmp28 = load i64, i64* %tmp27, align 8, !tbaa !2
  %tmp29 = getelementptr inbounds double, double* %arg, i64 3
  %tmp30 = bitcast double* %tmp29 to i64*
  store i64 %tmp28, i64* %tmp30, align 8, !tbaa !2
  %tmp31 = shl i32 %arg3, 2
  %tmp32 = add nsw i32 %tmp31, %arg2
  %tmp33 = sext i32 %tmp32 to i64
  %tmp34 = getelementptr inbounds double, double* %arg1, i64 %tmp33
  %tmp35 = bitcast double* %tmp34 to i64*
  %tmp36 = load i64, i64* %tmp35, align 8, !tbaa !2
  %tmp37 = getelementptr inbounds double, double* %arg, i64 4
  %tmp38 = bitcast double* %tmp37 to i64*
  store i64 %tmp36, i64* %tmp38, align 8, !tbaa !2
  %tmp39 = mul nsw i32 %arg3, 5
  %tmp40 = add nsw i32 %tmp39, %arg2
  %tmp41 = sext i32 %tmp40 to i64
  %tmp42 = getelementptr inbounds double, double* %arg1, i64 %tmp41
  %tmp43 = bitcast double* %tmp42 to i64*
  %tmp44 = load i64, i64* %tmp43, align 8, !tbaa !2
  %tmp45 = getelementptr inbounds double, double* %arg, i64 5
  %tmp46 = bitcast double* %tmp45 to i64*
  store i64 %tmp44, i64* %tmp46, align 8, !tbaa !2
  %tmp47 = mul nsw i32 %arg3, 6
  %tmp48 = add nsw i32 %tmp47, %arg2
  %tmp49 = sext i32 %tmp48 to i64
  %tmp50 = getelementptr inbounds double, double* %arg1, i64 %tmp49
  %tmp51 = bitcast double* %tmp50 to i64*
  %tmp52 = load i64, i64* %tmp51, align 8, !tbaa !2
  %tmp53 = getelementptr inbounds double, double* %arg, i64 6
  %tmp54 = bitcast double* %tmp53 to i64*
  store i64 %tmp52, i64* %tmp54, align 8, !tbaa !2
  %tmp55 = mul nsw i32 %arg3, 7
  %tmp56 = add nsw i32 %tmp55, %arg2
  %tmp57 = sext i32 %tmp56 to i64
  %tmp58 = getelementptr inbounds double, double* %arg1, i64 %tmp57
  %tmp59 = bitcast double* %tmp58 to i64*
  %tmp60 = load i64, i64* %tmp59, align 8, !tbaa !2
  %tmp61 = getelementptr inbounds double, double* %arg, i64 7
  %tmp62 = bitcast double* %tmp61 to i64*
  store i64 %tmp60, i64* %tmp62, align 8, !tbaa !2
  ret void
}

; Function Attrs: norecurse nounwind uwtable
define internal dso_local void @loady8(double* nocapture %arg, double* nocapture readonly %arg1, i32 %arg2, i32 %arg3) #2 {
bb:
  %tmp = sext i32 %arg2 to i64
  %tmp4 = getelementptr inbounds double, double* %arg1, i64 %tmp
  %tmp5 = bitcast double* %tmp4 to i64*
  %tmp6 = load i64, i64* %tmp5, align 8, !tbaa !2
  %tmp7 = bitcast double* %arg to i64*
  store i64 %tmp6, i64* %tmp7, align 8, !tbaa !2
  %tmp8 = add nsw i32 %arg3, %arg2
  %tmp9 = sext i32 %tmp8 to i64
  %tmp10 = getelementptr inbounds double, double* %arg1, i64 %tmp9
  %tmp11 = bitcast double* %tmp10 to i64*
  %tmp12 = load i64, i64* %tmp11, align 8, !tbaa !2
  %tmp13 = getelementptr inbounds double, double* %arg, i64 1
  %tmp14 = bitcast double* %tmp13 to i64*
  store i64 %tmp12, i64* %tmp14, align 8, !tbaa !2
  %tmp15 = shl i32 %arg3, 1
  %tmp16 = add nsw i32 %tmp15, %arg2
  %tmp17 = sext i32 %tmp16 to i64
  %tmp18 = getelementptr inbounds double, double* %arg1, i64 %tmp17
  %tmp19 = bitcast double* %tmp18 to i64*
  %tmp20 = load i64, i64* %tmp19, align 8, !tbaa !2
  %tmp21 = getelementptr inbounds double, double* %arg, i64 2
  %tmp22 = bitcast double* %tmp21 to i64*
  store i64 %tmp20, i64* %tmp22, align 8, !tbaa !2
  %tmp23 = mul nsw i32 %arg3, 3
  %tmp24 = add nsw i32 %tmp23, %arg2
  %tmp25 = sext i32 %tmp24 to i64
  %tmp26 = getelementptr inbounds double, double* %arg1, i64 %tmp25
  %tmp27 = bitcast double* %tmp26 to i64*
  %tmp28 = load i64, i64* %tmp27, align 8, !tbaa !2
  %tmp29 = getelementptr inbounds double, double* %arg, i64 3
  %tmp30 = bitcast double* %tmp29 to i64*
  store i64 %tmp28, i64* %tmp30, align 8, !tbaa !2
  %tmp31 = shl i32 %arg3, 2
  %tmp32 = add nsw i32 %tmp31, %arg2
  %tmp33 = sext i32 %tmp32 to i64
  %tmp34 = getelementptr inbounds double, double* %arg1, i64 %tmp33
  %tmp35 = bitcast double* %tmp34 to i64*
  %tmp36 = load i64, i64* %tmp35, align 8, !tbaa !2
  %tmp37 = getelementptr inbounds double, double* %arg, i64 4
  %tmp38 = bitcast double* %tmp37 to i64*
  store i64 %tmp36, i64* %tmp38, align 8, !tbaa !2
  %tmp39 = mul nsw i32 %arg3, 5
  %tmp40 = add nsw i32 %tmp39, %arg2
  %tmp41 = sext i32 %tmp40 to i64
  %tmp42 = getelementptr inbounds double, double* %arg1, i64 %tmp41
  %tmp43 = bitcast double* %tmp42 to i64*
  %tmp44 = load i64, i64* %tmp43, align 8, !tbaa !2
  %tmp45 = getelementptr inbounds double, double* %arg, i64 5
  %tmp46 = bitcast double* %tmp45 to i64*
  store i64 %tmp44, i64* %tmp46, align 8, !tbaa !2
  %tmp47 = mul nsw i32 %arg3, 6
  %tmp48 = add nsw i32 %tmp47, %arg2
  %tmp49 = sext i32 %tmp48 to i64
  %tmp50 = getelementptr inbounds double, double* %arg1, i64 %tmp49
  %tmp51 = bitcast double* %tmp50 to i64*
  %tmp52 = load i64, i64* %tmp51, align 8, !tbaa !2
  %tmp53 = getelementptr inbounds double, double* %arg, i64 6
  %tmp54 = bitcast double* %tmp53 to i64*
  store i64 %tmp52, i64* %tmp54, align 8, !tbaa !2
  %tmp55 = mul nsw i32 %arg3, 7
  %tmp56 = add nsw i32 %tmp55, %arg2
  %tmp57 = sext i32 %tmp56 to i64
  %tmp58 = getelementptr inbounds double, double* %arg1, i64 %tmp57
  %tmp59 = bitcast double* %tmp58 to i64*
  %tmp60 = load i64, i64* %tmp59, align 8, !tbaa !2
  %tmp61 = getelementptr inbounds double, double* %arg, i64 7
  %tmp62 = bitcast double* %tmp61 to i64*
  store i64 %tmp60, i64* %tmp62, align 8, !tbaa !2
  ret void
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @fft1D_512(double* nocapture %arg, double* nocapture %arg1) #3 {
bb:
  %tmp = alloca [512 x double], align 16
  %tmp2 = alloca [512 x double], align 16
  %tmp3 = alloca [576 x double], align 16
  %tmp4 = bitcast [512 x double]* %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 4096, i8* nonnull %tmp4) #10
  %tmp5 = bitcast [512 x double]* %tmp2 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4096, i8* nonnull %tmp5) #10
  %tmp6 = bitcast [576 x double]* %tmp3 to i8*
  call void @llvm.lifetime.start.p0i8(i64 4608, i8* nonnull %tmp6) #10
  br label %bb7

bb7:                                              ; preds = %bb7, %bb
  %tmp8 = phi i64 [ 0, %bb ], [ %tmp211, %bb7 ]
  %tmp9 = getelementptr inbounds double, double* %arg, i64 %tmp8
  %tmp10 = load double, double* %tmp9, align 8, !tbaa !2
  %tmp11 = add nuw nsw i64 %tmp8, 64
  %tmp12 = getelementptr inbounds double, double* %arg, i64 %tmp11
  %tmp13 = load double, double* %tmp12, align 8, !tbaa !2
  %tmp14 = add nuw nsw i64 %tmp8, 128
  %tmp15 = getelementptr inbounds double, double* %arg, i64 %tmp14
  %tmp16 = load double, double* %tmp15, align 8, !tbaa !2
  %tmp17 = add nuw nsw i64 %tmp8, 192
  %tmp18 = getelementptr inbounds double, double* %arg, i64 %tmp17
  %tmp19 = load double, double* %tmp18, align 8, !tbaa !2
  %tmp20 = add nuw nsw i64 %tmp8, 256
  %tmp21 = getelementptr inbounds double, double* %arg, i64 %tmp20
  %tmp22 = load double, double* %tmp21, align 8, !tbaa !2
  %tmp23 = add nuw nsw i64 %tmp8, 320
  %tmp24 = getelementptr inbounds double, double* %arg, i64 %tmp23
  %tmp25 = load double, double* %tmp24, align 8, !tbaa !2
  %tmp26 = add nuw nsw i64 %tmp8, 384
  %tmp27 = getelementptr inbounds double, double* %arg, i64 %tmp26
  %tmp28 = load double, double* %tmp27, align 8, !tbaa !2
  %tmp29 = add nuw nsw i64 %tmp8, 448
  %tmp30 = getelementptr inbounds double, double* %arg, i64 %tmp29
  %tmp31 = load double, double* %tmp30, align 8, !tbaa !2
  %tmp32 = getelementptr inbounds double, double* %arg1, i64 %tmp8
  %tmp33 = load double, double* %tmp32, align 8, !tbaa !2
  %tmp34 = getelementptr inbounds double, double* %arg1, i64 %tmp11
  %tmp35 = load double, double* %tmp34, align 8, !tbaa !2
  %tmp36 = getelementptr inbounds double, double* %arg1, i64 %tmp14
  %tmp37 = load double, double* %tmp36, align 8, !tbaa !2
  %tmp38 = getelementptr inbounds double, double* %arg1, i64 %tmp17
  %tmp39 = load double, double* %tmp38, align 8, !tbaa !2
  %tmp40 = getelementptr inbounds double, double* %arg1, i64 %tmp20
  %tmp41 = load double, double* %tmp40, align 8, !tbaa !2
  %tmp42 = getelementptr inbounds double, double* %arg1, i64 %tmp23
  %tmp43 = load double, double* %tmp42, align 8, !tbaa !2
  %tmp44 = getelementptr inbounds double, double* %arg1, i64 %tmp26
  %tmp45 = load double, double* %tmp44, align 8, !tbaa !2
  %tmp46 = getelementptr inbounds double, double* %arg1, i64 %tmp29
  %tmp47 = load double, double* %tmp46, align 8, !tbaa !2
  %tmp48 = fadd double %tmp10, %tmp22
  %tmp49 = fadd double %tmp33, %tmp41
  %tmp50 = fsub double %tmp10, %tmp22
  %tmp51 = fsub double %tmp33, %tmp41
  %tmp52 = fadd double %tmp13, %tmp25
  %tmp53 = fadd double %tmp35, %tmp43
  %tmp54 = fsub double %tmp13, %tmp25
  %tmp55 = fsub double %tmp35, %tmp43
  %tmp56 = fadd double %tmp16, %tmp28
  %tmp57 = fadd double %tmp37, %tmp45
  %tmp58 = fsub double %tmp16, %tmp28
  %tmp59 = fsub double %tmp37, %tmp45
  %tmp60 = fadd double %tmp19, %tmp31
  %tmp61 = fadd double %tmp39, %tmp47
  %tmp62 = fsub double %tmp19, %tmp31
  %tmp63 = fsub double %tmp39, %tmp47
  %tmp64 = fadd double %tmp54, %tmp55
  %tmp65 = fmul double %tmp64, 0x3FE6A09E667F3BCD
  %tmp66 = fsub double %tmp55, %tmp54
  %tmp67 = fmul double %tmp66, 0x3FE6A09E667F3BCD
  %tmp68 = fmul double %tmp58, 0.000000e+00
  %tmp69 = fadd double %tmp68, %tmp59
  %tmp70 = fmul double %tmp59, 0.000000e+00
  %tmp71 = fsub double %tmp70, %tmp58
  %tmp72 = fsub double %tmp63, %tmp62
  %tmp73 = fmul double %tmp72, 0x3FE6A09E667F3BCD
  %tmp74 = fsub double -0.000000e+00, %tmp63
  %tmp75 = fsub double %tmp74, %tmp62
  %tmp76 = fmul double %tmp75, 0x3FE6A09E667F3BCD
  %tmp77 = fadd double %tmp48, %tmp56
  %tmp78 = fadd double %tmp49, %tmp57
  %tmp79 = fsub double %tmp48, %tmp56
  %tmp80 = fsub double %tmp49, %tmp57
  %tmp81 = fadd double %tmp52, %tmp60
  %tmp82 = fadd double %tmp53, %tmp61
  %tmp83 = fsub double %tmp52, %tmp60
  %tmp84 = fsub double %tmp53, %tmp61
  %tmp85 = fmul double %tmp83, 0.000000e+00
  %tmp86 = fadd double %tmp85, %tmp84
  %tmp87 = fsub double -0.000000e+00, %tmp83
  %tmp88 = fmul double %tmp84, 0.000000e+00
  %tmp89 = fsub double %tmp87, %tmp88
  %tmp90 = fadd double %tmp77, %tmp81
  %tmp91 = fsub double %tmp77, %tmp81
  %tmp92 = fsub double %tmp78, %tmp82
  %tmp93 = fadd double %tmp79, %tmp86
  %tmp94 = fadd double %tmp80, %tmp89
  %tmp95 = fsub double %tmp79, %tmp86
  %tmp96 = fsub double %tmp80, %tmp89
  %tmp97 = fadd double %tmp50, %tmp69
  %tmp98 = fadd double %tmp51, %tmp71
  %tmp99 = fsub double %tmp50, %tmp69
  %tmp100 = fsub double %tmp51, %tmp71
  %tmp101 = fadd double %tmp65, %tmp73
  %tmp102 = fadd double %tmp67, %tmp76
  %tmp103 = fsub double %tmp65, %tmp73
  %tmp104 = fsub double %tmp67, %tmp76
  %tmp105 = fmul double %tmp103, 0.000000e+00
  %tmp106 = fadd double %tmp104, %tmp105
  %tmp107 = fsub double -0.000000e+00, %tmp103
  %tmp108 = fmul double %tmp104, 0.000000e+00
  %tmp109 = fsub double %tmp107, %tmp108
  %tmp110 = fadd double %tmp97, %tmp101
  %tmp111 = fadd double %tmp98, %tmp102
  %tmp112 = fsub double %tmp97, %tmp101
  %tmp113 = fsub double %tmp98, %tmp102
  %tmp114 = fadd double %tmp99, %tmp106
  %tmp115 = fadd double %tmp100, %tmp109
  %tmp116 = fsub double %tmp99, %tmp106
  %tmp117 = fsub double %tmp100, %tmp109
  %tmp118 = trunc i64 %tmp8 to i32
  %tmp119 = sitofp i32 %tmp118 to double
  %tmp120 = fmul double %tmp119, 0xBFA921FB54411744
  %tmp121 = tail call double @cos(double %tmp120) #10
  %tmp122 = tail call double @sin(double %tmp120) #10
  %tmp123 = fmul double %tmp91, %tmp121
  %tmp124 = fmul double %tmp122, %tmp92
  %tmp125 = fsub double %tmp123, %tmp124
  %tmp126 = fmul double %tmp91, %tmp122
  %tmp127 = fmul double %tmp121, %tmp92
  %tmp128 = insertelement <2 x double> undef, double %tmp78, i32 0
  %tmp129 = insertelement <2 x double> %tmp128, double %tmp126, i32 1
  %tmp130 = insertelement <2 x double> undef, double %tmp82, i32 0
  %tmp131 = insertelement <2 x double> %tmp130, double %tmp127, i32 1
  %tmp132 = fadd <2 x double> %tmp129, %tmp131
  %tmp133 = fmul double %tmp119, 0xBF9921FB54411744
  %tmp134 = tail call double @cos(double %tmp133) #10
  %tmp135 = tail call double @sin(double %tmp133) #10
  %tmp136 = fmul double %tmp134, %tmp93
  %tmp137 = fmul double %tmp135, %tmp94
  %tmp138 = fsub double %tmp136, %tmp137
  %tmp139 = fmul double %tmp135, %tmp93
  %tmp140 = fmul double %tmp134, %tmp94
  %tmp141 = fadd double %tmp139, %tmp140
  %tmp142 = fmul double %tmp119, 0xBFB2D97C7F30D173
  %tmp143 = tail call double @cos(double %tmp142) #10
  %tmp144 = tail call double @sin(double %tmp142) #10
  %tmp145 = fmul double %tmp95, %tmp143
  %tmp146 = fmul double %tmp96, %tmp144
  %tmp147 = fsub double %tmp145, %tmp146
  %tmp148 = fmul double %tmp95, %tmp144
  %tmp149 = fmul double %tmp143, %tmp96
  %tmp150 = fadd double %tmp149, %tmp148
  %tmp151 = fmul double %tmp119, 0xBF8921FB54411744
  %tmp152 = tail call double @cos(double %tmp151) #10
  %tmp153 = tail call double @sin(double %tmp151) #10
  %tmp154 = fmul double %tmp110, %tmp152
  %tmp155 = fmul double %tmp111, %tmp153
  %tmp156 = fsub double %tmp154, %tmp155
  %tmp157 = fmul double %tmp110, %tmp153
  %tmp158 = fmul double %tmp111, %tmp152
  %tmp159 = fadd double %tmp158, %tmp157
  %tmp160 = fmul double %tmp119, 0xBFAF6A7A29515D15
  %tmp161 = tail call double @cos(double %tmp160) #10
  %tmp162 = tail call double @sin(double %tmp160) #10
  %tmp163 = fmul double %tmp112, %tmp161
  %tmp164 = fmul double %tmp113, %tmp162
  %tmp165 = fsub double %tmp163, %tmp164
  %tmp166 = fmul double %tmp112, %tmp162
  %tmp167 = fmul double %tmp113, %tmp161
  %tmp168 = fadd double %tmp167, %tmp166
  %tmp169 = fmul double %tmp119, 0xBFA2D97C7F30D173
  %tmp170 = tail call double @cos(double %tmp169) #10
  %tmp171 = tail call double @sin(double %tmp169) #10
  %tmp172 = fmul double %tmp114, %tmp170
  %tmp173 = fmul double %tmp115, %tmp171
  %tmp174 = fsub double %tmp172, %tmp173
  %tmp175 = fmul double %tmp114, %tmp171
  %tmp176 = fmul double %tmp115, %tmp170
  %tmp177 = fadd double %tmp176, %tmp175
  %tmp178 = fmul double %tmp119, 0xBFB5FDBBE9B8F45C
  %tmp179 = tail call double @cos(double %tmp178) #10
  %tmp180 = tail call double @sin(double %tmp178) #10
  %tmp181 = fmul double %tmp116, %tmp179
  %tmp182 = fmul double %tmp117, %tmp180
  %tmp183 = fsub double %tmp181, %tmp182
  %tmp184 = fmul double %tmp116, %tmp180
  %tmp185 = fmul double %tmp117, %tmp179
  %tmp186 = fadd double %tmp185, %tmp184
  %tmp187 = shl nsw i64 %tmp8, 3
  %tmp188 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp187
  store double %tmp90, double* %tmp188, align 16, !tbaa !2
  %tmp189 = or i64 %tmp187, 1
  %tmp190 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp189
  store double %tmp125, double* %tmp190, align 8, !tbaa !2
  %tmp191 = or i64 %tmp187, 2
  %tmp192 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp191
  store double %tmp138, double* %tmp192, align 16, !tbaa !2
  %tmp193 = or i64 %tmp187, 3
  %tmp194 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp193
  store double %tmp147, double* %tmp194, align 8, !tbaa !2
  %tmp195 = or i64 %tmp187, 4
  %tmp196 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp195
  store double %tmp156, double* %tmp196, align 16, !tbaa !2
  %tmp197 = or i64 %tmp187, 5
  %tmp198 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp197
  store double %tmp165, double* %tmp198, align 8, !tbaa !2
  %tmp199 = or i64 %tmp187, 6
  %tmp200 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp199
  store double %tmp174, double* %tmp200, align 16, !tbaa !2
  %tmp201 = or i64 %tmp187, 7
  %tmp202 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp201
  store double %tmp183, double* %tmp202, align 8, !tbaa !2
  %tmp203 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp187
  %tmp204 = bitcast double* %tmp203 to <2 x double>*
  store <2 x double> %tmp132, <2 x double>* %tmp204, align 16, !tbaa !2
  %tmp205 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp191
  store double %tmp141, double* %tmp205, align 16, !tbaa !2
  %tmp206 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp193
  store double %tmp150, double* %tmp206, align 8, !tbaa !2
  %tmp207 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp195
  store double %tmp159, double* %tmp207, align 16, !tbaa !2
  %tmp208 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp197
  store double %tmp168, double* %tmp208, align 8, !tbaa !2
  %tmp209 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp199
  store double %tmp177, double* %tmp209, align 16, !tbaa !2
  %tmp210 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp201
  store double %tmp186, double* %tmp210, align 8, !tbaa !2
  %tmp211 = add nuw nsw i64 %tmp8, 1
  %tmp212 = icmp eq i64 %tmp211, 64
  br i1 %tmp212, label %bb213, label %bb7

bb213:                                            ; preds = %bb7
  br label %bb214

bb214:                                            ; preds = %bb214, %bb213
  %tmp215 = phi i64 [ %tmp251, %bb214 ], [ 0, %bb213 ]
  %tmp216 = shl nsw i64 %tmp215, 3
  %tmp217 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp216
  %tmp218 = bitcast double* %tmp217 to <16 x i64>*
  %tmp219 = load <16 x i64>, <16 x i64>* %tmp218, align 16, !tbaa !2
  %tmp220 = shufflevector <16 x i64> %tmp219, <16 x i64> undef, <2 x i32> <i32 0, i32 8>
  %tmp221 = shufflevector <16 x i64> %tmp219, <16 x i64> undef, <2 x i32> <i32 1, i32 9>
  %tmp222 = shufflevector <16 x i64> %tmp219, <16 x i64> undef, <2 x i32> <i32 2, i32 10>
  %tmp223 = shufflevector <16 x i64> %tmp219, <16 x i64> undef, <2 x i32> <i32 3, i32 11>
  %tmp224 = shufflevector <16 x i64> %tmp219, <16 x i64> undef, <2 x i32> <i32 4, i32 12>
  %tmp225 = shufflevector <16 x i64> %tmp219, <16 x i64> undef, <2 x i32> <i32 5, i32 13>
  %tmp226 = shufflevector <16 x i64> %tmp219, <16 x i64> undef, <2 x i32> <i32 6, i32 14>
  %tmp227 = shufflevector <16 x i64> %tmp219, <16 x i64> undef, <2 x i32> <i32 7, i32 15>
  %tmp228 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp215
  %tmp229 = bitcast double* %tmp228 to <2 x i64>*
  store <2 x i64> %tmp220, <2 x i64>* %tmp229, align 16, !tbaa !2
  %tmp230 = add nuw nsw i64 %tmp215, 264
  %tmp231 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp230
  %tmp232 = bitcast double* %tmp231 to <2 x i64>*
  store <2 x i64> %tmp221, <2 x i64>* %tmp232, align 16, !tbaa !2
  %tmp233 = add nuw nsw i64 %tmp215, 66
  %tmp234 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp233
  %tmp235 = bitcast double* %tmp234 to <2 x i64>*
  store <2 x i64> %tmp224, <2 x i64>* %tmp235, align 16, !tbaa !2
  %tmp236 = add nuw nsw i64 %tmp215, 330
  %tmp237 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp236
  %tmp238 = bitcast double* %tmp237 to <2 x i64>*
  store <2 x i64> %tmp225, <2 x i64>* %tmp238, align 16, !tbaa !2
  %tmp239 = add nuw nsw i64 %tmp215, 132
  %tmp240 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp239
  %tmp241 = bitcast double* %tmp240 to <2 x i64>*
  store <2 x i64> %tmp222, <2 x i64>* %tmp241, align 16, !tbaa !2
  %tmp242 = add nuw nsw i64 %tmp215, 396
  %tmp243 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp242
  %tmp244 = bitcast double* %tmp243 to <2 x i64>*
  store <2 x i64> %tmp223, <2 x i64>* %tmp244, align 16, !tbaa !2
  %tmp245 = add nuw nsw i64 %tmp215, 198
  %tmp246 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp245
  %tmp247 = bitcast double* %tmp246 to <2 x i64>*
  store <2 x i64> %tmp226, <2 x i64>* %tmp247, align 16, !tbaa !2
  %tmp248 = add nuw nsw i64 %tmp215, 462
  %tmp249 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp248
  %tmp250 = bitcast double* %tmp249 to <2 x i64>*
  store <2 x i64> %tmp227, <2 x i64>* %tmp250, align 16, !tbaa !2
  %tmp251 = add i64 %tmp215, 2
  %tmp252 = icmp eq i64 %tmp251, 64
  br i1 %tmp252, label %bb253, label %bb214, !llvm.loop !6

bb253:                                            ; preds = %bb214
  br label %bb254

bb254:                                            ; preds = %bb254, %bb253
  %tmp255 = phi i64 [ %tmp324, %bb254 ], [ 0, %bb253 ]
  %tmp256 = trunc i64 %tmp255 to i32
  %tmp257 = lshr i32 %tmp256, 3
  %tmp258 = and i32 %tmp256, 7
  %tmp259 = mul nuw nsw i32 %tmp258, 66
  %tmp260 = add nuw nsw i32 %tmp259, %tmp257
  %tmp261 = zext i32 %tmp260 to i64
  %tmp262 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp261
  %tmp263 = bitcast double* %tmp262 to i64*
  %tmp264 = load i64, i64* %tmp263, align 8, !tbaa !2
  %tmp265 = shl nsw i64 %tmp255, 3
  %tmp266 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp265
  %tmp267 = bitcast double* %tmp266 to i64*
  store i64 %tmp264, i64* %tmp267, align 16, !tbaa !2
  %tmp268 = add nuw nsw i32 %tmp260, 32
  %tmp269 = zext i32 %tmp268 to i64
  %tmp270 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp269
  %tmp271 = bitcast double* %tmp270 to i64*
  %tmp272 = load i64, i64* %tmp271, align 8, !tbaa !2
  %tmp273 = or i64 %tmp265, 4
  %tmp274 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp273
  %tmp275 = bitcast double* %tmp274 to i64*
  store i64 %tmp272, i64* %tmp275, align 16, !tbaa !2
  %tmp276 = add nuw nsw i32 %tmp260, 8
  %tmp277 = zext i32 %tmp276 to i64
  %tmp278 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp277
  %tmp279 = bitcast double* %tmp278 to i64*
  %tmp280 = load i64, i64* %tmp279, align 8, !tbaa !2
  %tmp281 = or i64 %tmp265, 1
  %tmp282 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp281
  %tmp283 = bitcast double* %tmp282 to i64*
  store i64 %tmp280, i64* %tmp283, align 8, !tbaa !2
  %tmp284 = add nuw nsw i32 %tmp260, 40
  %tmp285 = zext i32 %tmp284 to i64
  %tmp286 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp285
  %tmp287 = bitcast double* %tmp286 to i64*
  %tmp288 = load i64, i64* %tmp287, align 8, !tbaa !2
  %tmp289 = or i64 %tmp265, 5
  %tmp290 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp289
  %tmp291 = bitcast double* %tmp290 to i64*
  store i64 %tmp288, i64* %tmp291, align 8, !tbaa !2
  %tmp292 = add nuw nsw i32 %tmp260, 16
  %tmp293 = zext i32 %tmp292 to i64
  %tmp294 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp293
  %tmp295 = bitcast double* %tmp294 to i64*
  %tmp296 = load i64, i64* %tmp295, align 8, !tbaa !2
  %tmp297 = or i64 %tmp265, 2
  %tmp298 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp297
  %tmp299 = bitcast double* %tmp298 to i64*
  store i64 %tmp296, i64* %tmp299, align 16, !tbaa !2
  %tmp300 = add nuw nsw i32 %tmp260, 48
  %tmp301 = zext i32 %tmp300 to i64
  %tmp302 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp301
  %tmp303 = bitcast double* %tmp302 to i64*
  %tmp304 = load i64, i64* %tmp303, align 8, !tbaa !2
  %tmp305 = or i64 %tmp265, 6
  %tmp306 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp305
  %tmp307 = bitcast double* %tmp306 to i64*
  store i64 %tmp304, i64* %tmp307, align 16, !tbaa !2
  %tmp308 = add nuw nsw i32 %tmp260, 24
  %tmp309 = zext i32 %tmp308 to i64
  %tmp310 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp309
  %tmp311 = bitcast double* %tmp310 to i64*
  %tmp312 = load i64, i64* %tmp311, align 8, !tbaa !2
  %tmp313 = or i64 %tmp265, 3
  %tmp314 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp313
  %tmp315 = bitcast double* %tmp314 to i64*
  store i64 %tmp312, i64* %tmp315, align 8, !tbaa !2
  %tmp316 = add nuw nsw i32 %tmp260, 56
  %tmp317 = zext i32 %tmp316 to i64
  %tmp318 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp317
  %tmp319 = bitcast double* %tmp318 to i64*
  %tmp320 = load i64, i64* %tmp319, align 8, !tbaa !2
  %tmp321 = or i64 %tmp265, 7
  %tmp322 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp321
  %tmp323 = bitcast double* %tmp322 to i64*
  store i64 %tmp320, i64* %tmp323, align 8, !tbaa !2
  %tmp324 = add nuw nsw i64 %tmp255, 1
  %tmp325 = icmp eq i64 %tmp324, 64
  br i1 %tmp325, label %bb326, label %bb254

bb326:                                            ; preds = %bb254
  br label %bb327

bb327:                                            ; preds = %bb327, %bb326
  %tmp328 = phi i64 [ %tmp364, %bb327 ], [ 0, %bb326 ]
  %tmp329 = shl nsw i64 %tmp328, 3
  %tmp330 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp329
  %tmp331 = bitcast double* %tmp330 to <16 x i64>*
  %tmp332 = load <16 x i64>, <16 x i64>* %tmp331, align 16, !tbaa !2
  %tmp333 = shufflevector <16 x i64> %tmp332, <16 x i64> undef, <2 x i32> <i32 0, i32 8>
  %tmp334 = shufflevector <16 x i64> %tmp332, <16 x i64> undef, <2 x i32> <i32 1, i32 9>
  %tmp335 = shufflevector <16 x i64> %tmp332, <16 x i64> undef, <2 x i32> <i32 2, i32 10>
  %tmp336 = shufflevector <16 x i64> %tmp332, <16 x i64> undef, <2 x i32> <i32 3, i32 11>
  %tmp337 = shufflevector <16 x i64> %tmp332, <16 x i64> undef, <2 x i32> <i32 4, i32 12>
  %tmp338 = shufflevector <16 x i64> %tmp332, <16 x i64> undef, <2 x i32> <i32 5, i32 13>
  %tmp339 = shufflevector <16 x i64> %tmp332, <16 x i64> undef, <2 x i32> <i32 6, i32 14>
  %tmp340 = shufflevector <16 x i64> %tmp332, <16 x i64> undef, <2 x i32> <i32 7, i32 15>
  %tmp341 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp328
  %tmp342 = bitcast double* %tmp341 to <2 x i64>*
  store <2 x i64> %tmp333, <2 x i64>* %tmp342, align 16, !tbaa !2
  %tmp343 = add nuw nsw i64 %tmp328, 264
  %tmp344 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp343
  %tmp345 = bitcast double* %tmp344 to <2 x i64>*
  store <2 x i64> %tmp334, <2 x i64>* %tmp345, align 16, !tbaa !2
  %tmp346 = add nuw nsw i64 %tmp328, 66
  %tmp347 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp346
  %tmp348 = bitcast double* %tmp347 to <2 x i64>*
  store <2 x i64> %tmp337, <2 x i64>* %tmp348, align 16, !tbaa !2
  %tmp349 = add nuw nsw i64 %tmp328, 330
  %tmp350 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp349
  %tmp351 = bitcast double* %tmp350 to <2 x i64>*
  store <2 x i64> %tmp338, <2 x i64>* %tmp351, align 16, !tbaa !2
  %tmp352 = add nuw nsw i64 %tmp328, 132
  %tmp353 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp352
  %tmp354 = bitcast double* %tmp353 to <2 x i64>*
  store <2 x i64> %tmp335, <2 x i64>* %tmp354, align 16, !tbaa !2
  %tmp355 = add nuw nsw i64 %tmp328, 396
  %tmp356 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp355
  %tmp357 = bitcast double* %tmp356 to <2 x i64>*
  store <2 x i64> %tmp336, <2 x i64>* %tmp357, align 16, !tbaa !2
  %tmp358 = add nuw nsw i64 %tmp328, 198
  %tmp359 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp358
  %tmp360 = bitcast double* %tmp359 to <2 x i64>*
  store <2 x i64> %tmp339, <2 x i64>* %tmp360, align 16, !tbaa !2
  %tmp361 = add nuw nsw i64 %tmp328, 462
  %tmp362 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp361
  %tmp363 = bitcast double* %tmp362 to <2 x i64>*
  store <2 x i64> %tmp340, <2 x i64>* %tmp363, align 16, !tbaa !2
  %tmp364 = add i64 %tmp328, 2
  %tmp365 = icmp eq i64 %tmp364, 64
  br i1 %tmp365, label %bb366, label %bb327, !llvm.loop !8

bb366:                                            ; preds = %bb327
  br label %bb367

bb367:                                            ; preds = %bb367, %bb366
  %tmp368 = phi i64 [ %tmp437, %bb367 ], [ 0, %bb366 ]
  %tmp369 = shl nsw i64 %tmp368, 3
  %tmp370 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp369
  %tmp371 = bitcast double* %tmp370 to i64*
  %tmp372 = or i64 %tmp369, 1
  %tmp373 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp372
  %tmp374 = bitcast double* %tmp373 to i64*
  %tmp375 = or i64 %tmp369, 2
  %tmp376 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp375
  %tmp377 = bitcast double* %tmp376 to i64*
  %tmp378 = or i64 %tmp369, 3
  %tmp379 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp378
  %tmp380 = bitcast double* %tmp379 to i64*
  %tmp381 = or i64 %tmp369, 4
  %tmp382 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp381
  %tmp383 = bitcast double* %tmp382 to i64*
  %tmp384 = or i64 %tmp369, 5
  %tmp385 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp384
  %tmp386 = bitcast double* %tmp385 to i64*
  %tmp387 = or i64 %tmp369, 6
  %tmp388 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp387
  %tmp389 = bitcast double* %tmp388 to i64*
  %tmp390 = or i64 %tmp369, 7
  %tmp391 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp390
  %tmp392 = bitcast double* %tmp391 to i64*
  %tmp393 = trunc i64 %tmp368 to i32
  %tmp394 = lshr i32 %tmp393, 3
  %tmp395 = and i32 %tmp393, 7
  %tmp396 = mul nuw nsw i32 %tmp395, 66
  %tmp397 = add nuw nsw i32 %tmp396, %tmp394
  %tmp398 = zext i32 %tmp397 to i64
  %tmp399 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp398
  %tmp400 = bitcast double* %tmp399 to i64*
  %tmp401 = load i64, i64* %tmp400, align 8, !tbaa !2
  %tmp402 = add nuw nsw i32 %tmp397, 8
  %tmp403 = zext i32 %tmp402 to i64
  %tmp404 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp403
  %tmp405 = bitcast double* %tmp404 to i64*
  %tmp406 = load i64, i64* %tmp405, align 8, !tbaa !2
  %tmp407 = add nuw nsw i32 %tmp397, 16
  %tmp408 = zext i32 %tmp407 to i64
  %tmp409 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp408
  %tmp410 = bitcast double* %tmp409 to i64*
  %tmp411 = load i64, i64* %tmp410, align 8, !tbaa !2
  %tmp412 = add nuw nsw i32 %tmp397, 24
  %tmp413 = zext i32 %tmp412 to i64
  %tmp414 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp413
  %tmp415 = bitcast double* %tmp414 to i64*
  %tmp416 = load i64, i64* %tmp415, align 8, !tbaa !2
  %tmp417 = add nuw nsw i32 %tmp397, 32
  %tmp418 = zext i32 %tmp417 to i64
  %tmp419 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp418
  %tmp420 = bitcast double* %tmp419 to i64*
  %tmp421 = load i64, i64* %tmp420, align 8, !tbaa !2
  %tmp422 = add nuw nsw i32 %tmp397, 40
  %tmp423 = zext i32 %tmp422 to i64
  %tmp424 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp423
  %tmp425 = bitcast double* %tmp424 to i64*
  %tmp426 = load i64, i64* %tmp425, align 8, !tbaa !2
  %tmp427 = add nuw nsw i32 %tmp397, 48
  %tmp428 = zext i32 %tmp427 to i64
  %tmp429 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp428
  %tmp430 = bitcast double* %tmp429 to i64*
  %tmp431 = load i64, i64* %tmp430, align 8, !tbaa !2
  %tmp432 = add nuw nsw i32 %tmp397, 56
  %tmp433 = zext i32 %tmp432 to i64
  %tmp434 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp433
  %tmp435 = bitcast double* %tmp434 to i64*
  %tmp436 = load i64, i64* %tmp435, align 8, !tbaa !2
  store i64 %tmp401, i64* %tmp371, align 16, !tbaa !2
  store i64 %tmp406, i64* %tmp374, align 8, !tbaa !2
  store i64 %tmp411, i64* %tmp377, align 16, !tbaa !2
  store i64 %tmp416, i64* %tmp380, align 8, !tbaa !2
  store i64 %tmp421, i64* %tmp383, align 16, !tbaa !2
  store i64 %tmp426, i64* %tmp386, align 8, !tbaa !2
  store i64 %tmp431, i64* %tmp389, align 16, !tbaa !2
  store i64 %tmp436, i64* %tmp392, align 8, !tbaa !2
  %tmp437 = add nuw nsw i64 %tmp368, 1
  %tmp438 = icmp eq i64 %tmp437, 64
  br i1 %tmp438, label %bb439, label %bb367

bb439:                                            ; preds = %bb367
  br label %bb440

bb440:                                            ; preds = %bb440, %bb439
  %tmp441 = phi i64 [ %tmp623, %bb440 ], [ 0, %bb439 ]
  %tmp442 = shl nsw i64 %tmp441, 3
  %tmp443 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp442
  %tmp444 = load double, double* %tmp443, align 16, !tbaa !2
  %tmp445 = or i64 %tmp442, 1
  %tmp446 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp445
  %tmp447 = load double, double* %tmp446, align 8, !tbaa !2
  %tmp448 = or i64 %tmp442, 2
  %tmp449 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp448
  %tmp450 = load double, double* %tmp449, align 16, !tbaa !2
  %tmp451 = or i64 %tmp442, 3
  %tmp452 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp451
  %tmp453 = load double, double* %tmp452, align 8, !tbaa !2
  %tmp454 = or i64 %tmp442, 4
  %tmp455 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp454
  %tmp456 = load double, double* %tmp455, align 16, !tbaa !2
  %tmp457 = or i64 %tmp442, 5
  %tmp458 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp457
  %tmp459 = load double, double* %tmp458, align 8, !tbaa !2
  %tmp460 = or i64 %tmp442, 6
  %tmp461 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp460
  %tmp462 = load double, double* %tmp461, align 16, !tbaa !2
  %tmp463 = or i64 %tmp442, 7
  %tmp464 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp463
  %tmp465 = load double, double* %tmp464, align 8, !tbaa !2
  %tmp466 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp442
  %tmp467 = load double, double* %tmp466, align 16, !tbaa !2
  %tmp468 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp445
  %tmp469 = load double, double* %tmp468, align 8, !tbaa !2
  %tmp470 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp448
  %tmp471 = load double, double* %tmp470, align 16, !tbaa !2
  %tmp472 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp451
  %tmp473 = load double, double* %tmp472, align 8, !tbaa !2
  %tmp474 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp454
  %tmp475 = load double, double* %tmp474, align 16, !tbaa !2
  %tmp476 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp457
  %tmp477 = load double, double* %tmp476, align 8, !tbaa !2
  %tmp478 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp460
  %tmp479 = load double, double* %tmp478, align 16, !tbaa !2
  %tmp480 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp463
  %tmp481 = load double, double* %tmp480, align 8, !tbaa !2
  %tmp482 = fadd double %tmp444, %tmp456
  %tmp483 = fadd double %tmp467, %tmp475
  %tmp484 = fsub double %tmp444, %tmp456
  %tmp485 = fsub double %tmp467, %tmp475
  %tmp486 = fadd double %tmp447, %tmp459
  %tmp487 = fadd double %tmp469, %tmp477
  %tmp488 = fsub double %tmp447, %tmp459
  %tmp489 = fsub double %tmp469, %tmp477
  %tmp490 = fadd double %tmp450, %tmp462
  %tmp491 = fadd double %tmp471, %tmp479
  %tmp492 = fsub double %tmp450, %tmp462
  %tmp493 = fsub double %tmp471, %tmp479
  %tmp494 = fadd double %tmp453, %tmp465
  %tmp495 = fadd double %tmp473, %tmp481
  %tmp496 = fsub double %tmp453, %tmp465
  %tmp497 = fsub double %tmp473, %tmp481
  %tmp498 = fadd double %tmp488, %tmp489
  %tmp499 = fmul double %tmp498, 0x3FE6A09E667F3BCD
  %tmp500 = fsub double %tmp489, %tmp488
  %tmp501 = fmul double %tmp500, 0x3FE6A09E667F3BCD
  %tmp502 = fmul double %tmp492, 0.000000e+00
  %tmp503 = fadd double %tmp502, %tmp493
  %tmp504 = fmul double %tmp493, 0.000000e+00
  %tmp505 = fsub double %tmp504, %tmp492
  %tmp506 = fsub double %tmp497, %tmp496
  %tmp507 = fmul double %tmp506, 0x3FE6A09E667F3BCD
  %tmp508 = fsub double -0.000000e+00, %tmp497
  %tmp509 = fsub double %tmp508, %tmp496
  %tmp510 = fmul double %tmp509, 0x3FE6A09E667F3BCD
  %tmp511 = fadd double %tmp482, %tmp490
  %tmp512 = fadd double %tmp483, %tmp491
  %tmp513 = fsub double %tmp482, %tmp490
  %tmp514 = fsub double %tmp483, %tmp491
  %tmp515 = fadd double %tmp486, %tmp494
  %tmp516 = fadd double %tmp487, %tmp495
  %tmp517 = fsub double %tmp486, %tmp494
  %tmp518 = fsub double %tmp487, %tmp495
  %tmp519 = fmul double %tmp517, 0.000000e+00
  %tmp520 = fadd double %tmp519, %tmp518
  %tmp521 = fsub double -0.000000e+00, %tmp517
  %tmp522 = fmul double %tmp518, 0.000000e+00
  %tmp523 = fsub double %tmp521, %tmp522
  %tmp524 = fadd double %tmp511, %tmp515
  %tmp525 = fsub double %tmp511, %tmp515
  %tmp526 = fsub double %tmp512, %tmp516
  %tmp527 = fadd double %tmp513, %tmp520
  %tmp528 = fadd double %tmp514, %tmp523
  %tmp529 = fsub double %tmp513, %tmp520
  %tmp530 = fsub double %tmp514, %tmp523
  %tmp531 = fadd double %tmp484, %tmp503
  %tmp532 = fadd double %tmp485, %tmp505
  %tmp533 = fsub double %tmp484, %tmp503
  %tmp534 = fsub double %tmp485, %tmp505
  %tmp535 = fadd double %tmp499, %tmp507
  %tmp536 = fadd double %tmp501, %tmp510
  %tmp537 = fsub double %tmp499, %tmp507
  %tmp538 = fsub double %tmp501, %tmp510
  %tmp539 = fmul double %tmp537, 0.000000e+00
  %tmp540 = fadd double %tmp538, %tmp539
  %tmp541 = fsub double -0.000000e+00, %tmp537
  %tmp542 = fmul double %tmp538, 0.000000e+00
  %tmp543 = fsub double %tmp541, %tmp542
  %tmp544 = fadd double %tmp531, %tmp535
  %tmp545 = fadd double %tmp532, %tmp536
  %tmp546 = fsub double %tmp531, %tmp535
  %tmp547 = fsub double %tmp532, %tmp536
  %tmp548 = fadd double %tmp533, %tmp540
  %tmp549 = fadd double %tmp534, %tmp543
  %tmp550 = fsub double %tmp533, %tmp540
  %tmp551 = fsub double %tmp534, %tmp543
  %tmp552 = trunc i64 %tmp441 to i32
  %tmp553 = lshr i32 %tmp552, 3
  %tmp554 = sitofp i32 %tmp553 to double
  %tmp555 = fmul double %tmp554, 0xBFD921FB54411744
  %tmp556 = tail call double @cos(double %tmp555) #10
  %tmp557 = tail call double @sin(double %tmp555) #10
  %tmp558 = fmul double %tmp525, %tmp556
  %tmp559 = fmul double %tmp557, %tmp526
  %tmp560 = fsub double %tmp558, %tmp559
  %tmp561 = fmul double %tmp525, %tmp557
  %tmp562 = fmul double %tmp556, %tmp526
  %tmp563 = insertelement <2 x double> undef, double %tmp512, i32 0
  %tmp564 = insertelement <2 x double> %tmp563, double %tmp561, i32 1
  %tmp565 = insertelement <2 x double> undef, double %tmp516, i32 0
  %tmp566 = insertelement <2 x double> %tmp565, double %tmp562, i32 1
  %tmp567 = fadd <2 x double> %tmp564, %tmp566
  %tmp568 = fmul double %tmp554, 0xBFC921FB54411744
  %tmp569 = tail call double @cos(double %tmp568) #10
  %tmp570 = tail call double @sin(double %tmp568) #10
  %tmp571 = fmul double %tmp569, %tmp527
  %tmp572 = fmul double %tmp570, %tmp528
  %tmp573 = fsub double %tmp571, %tmp572
  %tmp574 = fmul double %tmp570, %tmp527
  %tmp575 = fmul double %tmp569, %tmp528
  %tmp576 = fadd double %tmp574, %tmp575
  %tmp577 = fmul double %tmp554, 0xBFE2D97C7F30D173
  %tmp578 = tail call double @cos(double %tmp577) #10
  %tmp579 = tail call double @sin(double %tmp577) #10
  %tmp580 = fmul double %tmp529, %tmp578
  %tmp581 = fmul double %tmp530, %tmp579
  %tmp582 = fsub double %tmp580, %tmp581
  %tmp583 = fmul double %tmp529, %tmp579
  %tmp584 = fmul double %tmp578, %tmp530
  %tmp585 = fadd double %tmp584, %tmp583
  %tmp586 = fmul double %tmp554, 0xBFB921FB54411744
  %tmp587 = tail call double @cos(double %tmp586) #10
  %tmp588 = tail call double @sin(double %tmp586) #10
  %tmp589 = fmul double %tmp544, %tmp587
  %tmp590 = fmul double %tmp545, %tmp588
  %tmp591 = fsub double %tmp589, %tmp590
  %tmp592 = fmul double %tmp544, %tmp588
  %tmp593 = fmul double %tmp545, %tmp587
  %tmp594 = fadd double %tmp593, %tmp592
  %tmp595 = fmul double %tmp554, 0xBFDF6A7A29515D15
  %tmp596 = tail call double @cos(double %tmp595) #10
  %tmp597 = tail call double @sin(double %tmp595) #10
  %tmp598 = fmul double %tmp546, %tmp596
  %tmp599 = fmul double %tmp547, %tmp597
  %tmp600 = fsub double %tmp598, %tmp599
  %tmp601 = fmul double %tmp546, %tmp597
  %tmp602 = fmul double %tmp547, %tmp596
  %tmp603 = fadd double %tmp602, %tmp601
  %tmp604 = fmul double %tmp554, 0xBFD2D97C7F30D173
  %tmp605 = tail call double @cos(double %tmp604) #10
  %tmp606 = tail call double @sin(double %tmp604) #10
  %tmp607 = fmul double %tmp548, %tmp605
  %tmp608 = fmul double %tmp549, %tmp606
  %tmp609 = fsub double %tmp607, %tmp608
  %tmp610 = fmul double %tmp548, %tmp606
  %tmp611 = fmul double %tmp549, %tmp605
  %tmp612 = fadd double %tmp611, %tmp610
  %tmp613 = fmul double %tmp554, 0xBFE5FDBBE9B8F45C
  %tmp614 = tail call double @cos(double %tmp613) #10
  %tmp615 = tail call double @sin(double %tmp613) #10
  %tmp616 = fmul double %tmp550, %tmp614
  %tmp617 = fmul double %tmp551, %tmp615
  %tmp618 = fsub double %tmp616, %tmp617
  %tmp619 = fmul double %tmp550, %tmp615
  %tmp620 = fmul double %tmp551, %tmp614
  %tmp621 = fadd double %tmp620, %tmp619
  store double %tmp524, double* %tmp443, align 16, !tbaa !2
  store double %tmp560, double* %tmp446, align 8, !tbaa !2
  store double %tmp573, double* %tmp449, align 16, !tbaa !2
  store double %tmp582, double* %tmp452, align 8, !tbaa !2
  store double %tmp591, double* %tmp455, align 16, !tbaa !2
  store double %tmp600, double* %tmp458, align 8, !tbaa !2
  store double %tmp609, double* %tmp461, align 16, !tbaa !2
  store double %tmp618, double* %tmp464, align 8, !tbaa !2
  %tmp622 = bitcast double* %tmp466 to <2 x double>*
  store <2 x double> %tmp567, <2 x double>* %tmp622, align 16, !tbaa !2
  store double %tmp576, double* %tmp470, align 16, !tbaa !2
  store double %tmp585, double* %tmp472, align 8, !tbaa !2
  store double %tmp594, double* %tmp474, align 16, !tbaa !2
  store double %tmp603, double* %tmp476, align 8, !tbaa !2
  store double %tmp612, double* %tmp478, align 16, !tbaa !2
  store double %tmp621, double* %tmp480, align 8, !tbaa !2
  %tmp623 = add nuw nsw i64 %tmp441, 1
  %tmp624 = icmp eq i64 %tmp623, 64
  br i1 %tmp624, label %bb625, label %bb440

bb625:                                            ; preds = %bb440
  br label %bb626

bb626:                                            ; preds = %bb626, %bb625
  %tmp627 = phi i64 [ %tmp663, %bb626 ], [ 0, %bb625 ]
  %tmp628 = shl nsw i64 %tmp627, 3
  %tmp629 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp628
  %tmp630 = bitcast double* %tmp629 to <16 x i64>*
  %tmp631 = load <16 x i64>, <16 x i64>* %tmp630, align 16, !tbaa !2
  %tmp632 = shufflevector <16 x i64> %tmp631, <16 x i64> undef, <2 x i32> <i32 0, i32 8>
  %tmp633 = shufflevector <16 x i64> %tmp631, <16 x i64> undef, <2 x i32> <i32 1, i32 9>
  %tmp634 = shufflevector <16 x i64> %tmp631, <16 x i64> undef, <2 x i32> <i32 2, i32 10>
  %tmp635 = shufflevector <16 x i64> %tmp631, <16 x i64> undef, <2 x i32> <i32 3, i32 11>
  %tmp636 = shufflevector <16 x i64> %tmp631, <16 x i64> undef, <2 x i32> <i32 4, i32 12>
  %tmp637 = shufflevector <16 x i64> %tmp631, <16 x i64> undef, <2 x i32> <i32 5, i32 13>
  %tmp638 = shufflevector <16 x i64> %tmp631, <16 x i64> undef, <2 x i32> <i32 6, i32 14>
  %tmp639 = shufflevector <16 x i64> %tmp631, <16 x i64> undef, <2 x i32> <i32 7, i32 15>
  %tmp640 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp627
  %tmp641 = bitcast double* %tmp640 to <2 x i64>*
  store <2 x i64> %tmp632, <2 x i64>* %tmp641, align 16, !tbaa !2
  %tmp642 = add nuw nsw i64 %tmp627, 288
  %tmp643 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp642
  %tmp644 = bitcast double* %tmp643 to <2 x i64>*
  store <2 x i64> %tmp633, <2 x i64>* %tmp644, align 16, !tbaa !2
  %tmp645 = add nuw nsw i64 %tmp627, 72
  %tmp646 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp645
  %tmp647 = bitcast double* %tmp646 to <2 x i64>*
  store <2 x i64> %tmp636, <2 x i64>* %tmp647, align 16, !tbaa !2
  %tmp648 = add nuw nsw i64 %tmp627, 360
  %tmp649 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp648
  %tmp650 = bitcast double* %tmp649 to <2 x i64>*
  store <2 x i64> %tmp637, <2 x i64>* %tmp650, align 16, !tbaa !2
  %tmp651 = add nuw nsw i64 %tmp627, 144
  %tmp652 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp651
  %tmp653 = bitcast double* %tmp652 to <2 x i64>*
  store <2 x i64> %tmp634, <2 x i64>* %tmp653, align 16, !tbaa !2
  %tmp654 = add nuw nsw i64 %tmp627, 432
  %tmp655 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp654
  %tmp656 = bitcast double* %tmp655 to <2 x i64>*
  store <2 x i64> %tmp635, <2 x i64>* %tmp656, align 16, !tbaa !2
  %tmp657 = add nuw nsw i64 %tmp627, 216
  %tmp658 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp657
  %tmp659 = bitcast double* %tmp658 to <2 x i64>*
  store <2 x i64> %tmp638, <2 x i64>* %tmp659, align 16, !tbaa !2
  %tmp660 = add nuw nsw i64 %tmp627, 504
  %tmp661 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp660
  %tmp662 = bitcast double* %tmp661 to <2 x i64>*
  store <2 x i64> %tmp639, <2 x i64>* %tmp662, align 16, !tbaa !2
  %tmp663 = add i64 %tmp627, 2
  %tmp664 = icmp eq i64 %tmp663, 64
  br i1 %tmp664, label %bb665, label %bb626, !llvm.loop !9

bb665:                                            ; preds = %bb626
  br label %bb666

bb666:                                            ; preds = %bb666, %bb665
  %tmp667 = phi i64 [ %tmp736, %bb666 ], [ 0, %bb665 ]
  %tmp668 = trunc i64 %tmp667 to i32
  %tmp669 = lshr i32 %tmp668, 3
  %tmp670 = and i32 %tmp668, 7
  %tmp671 = mul nsw i32 %tmp669, 72
  %tmp672 = or i32 %tmp671, %tmp670
  %tmp673 = zext i32 %tmp672 to i64
  %tmp674 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp673
  %tmp675 = bitcast double* %tmp674 to i64*
  %tmp676 = load i64, i64* %tmp675, align 8, !tbaa !2
  %tmp677 = shl nsw i64 %tmp667, 3
  %tmp678 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp677
  %tmp679 = bitcast double* %tmp678 to i64*
  store i64 %tmp676, i64* %tmp679, align 16, !tbaa !2
  %tmp680 = add nuw nsw i32 %tmp672, 32
  %tmp681 = zext i32 %tmp680 to i64
  %tmp682 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp681
  %tmp683 = bitcast double* %tmp682 to i64*
  %tmp684 = load i64, i64* %tmp683, align 8, !tbaa !2
  %tmp685 = or i64 %tmp677, 4
  %tmp686 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp685
  %tmp687 = bitcast double* %tmp686 to i64*
  store i64 %tmp684, i64* %tmp687, align 16, !tbaa !2
  %tmp688 = add nuw nsw i32 %tmp672, 8
  %tmp689 = zext i32 %tmp688 to i64
  %tmp690 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp689
  %tmp691 = bitcast double* %tmp690 to i64*
  %tmp692 = load i64, i64* %tmp691, align 8, !tbaa !2
  %tmp693 = or i64 %tmp677, 1
  %tmp694 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp693
  %tmp695 = bitcast double* %tmp694 to i64*
  store i64 %tmp692, i64* %tmp695, align 8, !tbaa !2
  %tmp696 = add nuw nsw i32 %tmp672, 40
  %tmp697 = zext i32 %tmp696 to i64
  %tmp698 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp697
  %tmp699 = bitcast double* %tmp698 to i64*
  %tmp700 = load i64, i64* %tmp699, align 8, !tbaa !2
  %tmp701 = or i64 %tmp677, 5
  %tmp702 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp701
  %tmp703 = bitcast double* %tmp702 to i64*
  store i64 %tmp700, i64* %tmp703, align 8, !tbaa !2
  %tmp704 = add nuw nsw i32 %tmp672, 16
  %tmp705 = zext i32 %tmp704 to i64
  %tmp706 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp705
  %tmp707 = bitcast double* %tmp706 to i64*
  %tmp708 = load i64, i64* %tmp707, align 8, !tbaa !2
  %tmp709 = or i64 %tmp677, 2
  %tmp710 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp709
  %tmp711 = bitcast double* %tmp710 to i64*
  store i64 %tmp708, i64* %tmp711, align 16, !tbaa !2
  %tmp712 = add nuw nsw i32 %tmp672, 48
  %tmp713 = zext i32 %tmp712 to i64
  %tmp714 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp713
  %tmp715 = bitcast double* %tmp714 to i64*
  %tmp716 = load i64, i64* %tmp715, align 8, !tbaa !2
  %tmp717 = or i64 %tmp677, 6
  %tmp718 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp717
  %tmp719 = bitcast double* %tmp718 to i64*
  store i64 %tmp716, i64* %tmp719, align 16, !tbaa !2
  %tmp720 = add nuw nsw i32 %tmp672, 24
  %tmp721 = zext i32 %tmp720 to i64
  %tmp722 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp721
  %tmp723 = bitcast double* %tmp722 to i64*
  %tmp724 = load i64, i64* %tmp723, align 8, !tbaa !2
  %tmp725 = or i64 %tmp677, 3
  %tmp726 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp725
  %tmp727 = bitcast double* %tmp726 to i64*
  store i64 %tmp724, i64* %tmp727, align 8, !tbaa !2
  %tmp728 = add nuw nsw i32 %tmp672, 56
  %tmp729 = zext i32 %tmp728 to i64
  %tmp730 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp729
  %tmp731 = bitcast double* %tmp730 to i64*
  %tmp732 = load i64, i64* %tmp731, align 8, !tbaa !2
  %tmp733 = or i64 %tmp677, 7
  %tmp734 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp733
  %tmp735 = bitcast double* %tmp734 to i64*
  store i64 %tmp732, i64* %tmp735, align 8, !tbaa !2
  %tmp736 = add nuw nsw i64 %tmp667, 1
  %tmp737 = icmp eq i64 %tmp736, 64
  br i1 %tmp737, label %bb738, label %bb666

bb738:                                            ; preds = %bb666
  br label %bb739

bb739:                                            ; preds = %bb739, %bb738
  %tmp740 = phi i64 [ %tmp776, %bb739 ], [ 0, %bb738 ]
  %tmp741 = shl nsw i64 %tmp740, 3
  %tmp742 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp741
  %tmp743 = bitcast double* %tmp742 to <16 x i64>*
  %tmp744 = load <16 x i64>, <16 x i64>* %tmp743, align 16, !tbaa !2
  %tmp745 = shufflevector <16 x i64> %tmp744, <16 x i64> undef, <2 x i32> <i32 0, i32 8>
  %tmp746 = shufflevector <16 x i64> %tmp744, <16 x i64> undef, <2 x i32> <i32 1, i32 9>
  %tmp747 = shufflevector <16 x i64> %tmp744, <16 x i64> undef, <2 x i32> <i32 2, i32 10>
  %tmp748 = shufflevector <16 x i64> %tmp744, <16 x i64> undef, <2 x i32> <i32 3, i32 11>
  %tmp749 = shufflevector <16 x i64> %tmp744, <16 x i64> undef, <2 x i32> <i32 4, i32 12>
  %tmp750 = shufflevector <16 x i64> %tmp744, <16 x i64> undef, <2 x i32> <i32 5, i32 13>
  %tmp751 = shufflevector <16 x i64> %tmp744, <16 x i64> undef, <2 x i32> <i32 6, i32 14>
  %tmp752 = shufflevector <16 x i64> %tmp744, <16 x i64> undef, <2 x i32> <i32 7, i32 15>
  %tmp753 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp740
  %tmp754 = bitcast double* %tmp753 to <2 x i64>*
  store <2 x i64> %tmp745, <2 x i64>* %tmp754, align 16, !tbaa !2
  %tmp755 = add nuw nsw i64 %tmp740, 288
  %tmp756 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp755
  %tmp757 = bitcast double* %tmp756 to <2 x i64>*
  store <2 x i64> %tmp746, <2 x i64>* %tmp757, align 16, !tbaa !2
  %tmp758 = add nuw nsw i64 %tmp740, 72
  %tmp759 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp758
  %tmp760 = bitcast double* %tmp759 to <2 x i64>*
  store <2 x i64> %tmp749, <2 x i64>* %tmp760, align 16, !tbaa !2
  %tmp761 = add nuw nsw i64 %tmp740, 360
  %tmp762 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp761
  %tmp763 = bitcast double* %tmp762 to <2 x i64>*
  store <2 x i64> %tmp750, <2 x i64>* %tmp763, align 16, !tbaa !2
  %tmp764 = add nuw nsw i64 %tmp740, 144
  %tmp765 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp764
  %tmp766 = bitcast double* %tmp765 to <2 x i64>*
  store <2 x i64> %tmp747, <2 x i64>* %tmp766, align 16, !tbaa !2
  %tmp767 = add nuw nsw i64 %tmp740, 432
  %tmp768 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp767
  %tmp769 = bitcast double* %tmp768 to <2 x i64>*
  store <2 x i64> %tmp748, <2 x i64>* %tmp769, align 16, !tbaa !2
  %tmp770 = add nuw nsw i64 %tmp740, 216
  %tmp771 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp770
  %tmp772 = bitcast double* %tmp771 to <2 x i64>*
  store <2 x i64> %tmp751, <2 x i64>* %tmp772, align 16, !tbaa !2
  %tmp773 = add nuw nsw i64 %tmp740, 504
  %tmp774 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp773
  %tmp775 = bitcast double* %tmp774 to <2 x i64>*
  store <2 x i64> %tmp752, <2 x i64>* %tmp775, align 16, !tbaa !2
  %tmp776 = add i64 %tmp740, 2
  %tmp777 = icmp eq i64 %tmp776, 64
  br i1 %tmp777, label %bb778, label %bb739, !llvm.loop !10

bb778:                                            ; preds = %bb739
  br label %bb779

bb779:                                            ; preds = %bb779, %bb778
  %tmp780 = phi i64 [ %tmp849, %bb779 ], [ 0, %bb778 ]
  %tmp781 = shl nsw i64 %tmp780, 3
  %tmp782 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp781
  %tmp783 = bitcast double* %tmp782 to i64*
  %tmp784 = or i64 %tmp781, 1
  %tmp785 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp784
  %tmp786 = bitcast double* %tmp785 to i64*
  %tmp787 = or i64 %tmp781, 2
  %tmp788 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp787
  %tmp789 = bitcast double* %tmp788 to i64*
  %tmp790 = or i64 %tmp781, 3
  %tmp791 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp790
  %tmp792 = bitcast double* %tmp791 to i64*
  %tmp793 = or i64 %tmp781, 4
  %tmp794 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp793
  %tmp795 = bitcast double* %tmp794 to i64*
  %tmp796 = or i64 %tmp781, 5
  %tmp797 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp796
  %tmp798 = bitcast double* %tmp797 to i64*
  %tmp799 = or i64 %tmp781, 6
  %tmp800 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp799
  %tmp801 = bitcast double* %tmp800 to i64*
  %tmp802 = or i64 %tmp781, 7
  %tmp803 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp802
  %tmp804 = bitcast double* %tmp803 to i64*
  %tmp805 = trunc i64 %tmp780 to i32
  %tmp806 = lshr i32 %tmp805, 3
  %tmp807 = and i32 %tmp805, 7
  %tmp808 = mul nsw i32 %tmp806, 72
  %tmp809 = or i32 %tmp808, %tmp807
  %tmp810 = zext i32 %tmp809 to i64
  %tmp811 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp810
  %tmp812 = bitcast double* %tmp811 to i64*
  %tmp813 = load i64, i64* %tmp812, align 8, !tbaa !2
  %tmp814 = add nuw nsw i32 %tmp809, 8
  %tmp815 = zext i32 %tmp814 to i64
  %tmp816 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp815
  %tmp817 = bitcast double* %tmp816 to i64*
  %tmp818 = load i64, i64* %tmp817, align 8, !tbaa !2
  %tmp819 = add nuw nsw i32 %tmp809, 16
  %tmp820 = zext i32 %tmp819 to i64
  %tmp821 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp820
  %tmp822 = bitcast double* %tmp821 to i64*
  %tmp823 = load i64, i64* %tmp822, align 8, !tbaa !2
  %tmp824 = add nuw nsw i32 %tmp809, 24
  %tmp825 = zext i32 %tmp824 to i64
  %tmp826 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp825
  %tmp827 = bitcast double* %tmp826 to i64*
  %tmp828 = load i64, i64* %tmp827, align 8, !tbaa !2
  %tmp829 = add nuw nsw i32 %tmp809, 32
  %tmp830 = zext i32 %tmp829 to i64
  %tmp831 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp830
  %tmp832 = bitcast double* %tmp831 to i64*
  %tmp833 = load i64, i64* %tmp832, align 8, !tbaa !2
  %tmp834 = add nuw nsw i32 %tmp809, 40
  %tmp835 = zext i32 %tmp834 to i64
  %tmp836 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp835
  %tmp837 = bitcast double* %tmp836 to i64*
  %tmp838 = load i64, i64* %tmp837, align 8, !tbaa !2
  %tmp839 = add nuw nsw i32 %tmp809, 48
  %tmp840 = zext i32 %tmp839 to i64
  %tmp841 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp840
  %tmp842 = bitcast double* %tmp841 to i64*
  %tmp843 = load i64, i64* %tmp842, align 8, !tbaa !2
  %tmp844 = add nuw nsw i32 %tmp809, 56
  %tmp845 = zext i32 %tmp844 to i64
  %tmp846 = getelementptr inbounds [576 x double], [576 x double]* %tmp3, i64 0, i64 %tmp845
  %tmp847 = bitcast double* %tmp846 to i64*
  %tmp848 = load i64, i64* %tmp847, align 8, !tbaa !2
  store i64 %tmp813, i64* %tmp783, align 16, !tbaa !2
  store i64 %tmp818, i64* %tmp786, align 8, !tbaa !2
  store i64 %tmp823, i64* %tmp789, align 16, !tbaa !2
  store i64 %tmp828, i64* %tmp792, align 8, !tbaa !2
  store i64 %tmp833, i64* %tmp795, align 16, !tbaa !2
  store i64 %tmp838, i64* %tmp798, align 8, !tbaa !2
  store i64 %tmp843, i64* %tmp801, align 16, !tbaa !2
  store i64 %tmp848, i64* %tmp804, align 8, !tbaa !2
  %tmp849 = add nuw nsw i64 %tmp780, 1
  %tmp850 = icmp eq i64 %tmp849, 64
  br i1 %tmp850, label %bb851, label %bb779

bb851:                                            ; preds = %bb779
  %tmp852 = getelementptr double, double* %arg, i64 512
  %tmp853 = getelementptr double, double* %arg1, i64 512
  %tmp854 = icmp ugt double* %tmp853, %arg
  %tmp855 = icmp ugt double* %tmp852, %arg1
  %tmp856 = and i1 %tmp854, %tmp855
  br i1 %tmp856, label %bb858, label %bb857

bb857:                                            ; preds = %bb851
  br label %bb859

bb858:                                            ; preds = %bb851
  br label %bb996

bb859:                                            ; preds = %bb859, %bb857
  %tmp860 = phi i64 [ %tmp994, %bb859 ], [ 0, %bb857 ]
  %tmp861 = shl nsw i64 %tmp860, 3
  %tmp862 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp861
  %tmp863 = bitcast double* %tmp862 to <16 x double>*
  %tmp864 = load <16 x double>, <16 x double>* %tmp863, align 16, !tbaa !2
  %tmp865 = shufflevector <16 x double> %tmp864, <16 x double> undef, <2 x i32> <i32 0, i32 8>
  %tmp866 = shufflevector <16 x double> %tmp864, <16 x double> undef, <2 x i32> <i32 1, i32 9>
  %tmp867 = shufflevector <16 x double> %tmp864, <16 x double> undef, <2 x i32> <i32 2, i32 10>
  %tmp868 = shufflevector <16 x double> %tmp864, <16 x double> undef, <2 x i32> <i32 3, i32 11>
  %tmp869 = shufflevector <16 x double> %tmp864, <16 x double> undef, <2 x i32> <i32 4, i32 12>
  %tmp870 = shufflevector <16 x double> %tmp864, <16 x double> undef, <2 x i32> <i32 5, i32 13>
  %tmp871 = shufflevector <16 x double> %tmp864, <16 x double> undef, <2 x i32> <i32 6, i32 14>
  %tmp872 = shufflevector <16 x double> %tmp864, <16 x double> undef, <2 x i32> <i32 7, i32 15>
  %tmp873 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp861
  %tmp874 = bitcast double* %tmp873 to <16 x double>*
  %tmp875 = load <16 x double>, <16 x double>* %tmp874, align 16, !tbaa !2
  %tmp876 = shufflevector <16 x double> %tmp875, <16 x double> undef, <2 x i32> <i32 0, i32 8>
  %tmp877 = shufflevector <16 x double> %tmp875, <16 x double> undef, <2 x i32> <i32 1, i32 9>
  %tmp878 = shufflevector <16 x double> %tmp875, <16 x double> undef, <2 x i32> <i32 2, i32 10>
  %tmp879 = shufflevector <16 x double> %tmp875, <16 x double> undef, <2 x i32> <i32 3, i32 11>
  %tmp880 = shufflevector <16 x double> %tmp875, <16 x double> undef, <2 x i32> <i32 4, i32 12>
  %tmp881 = shufflevector <16 x double> %tmp875, <16 x double> undef, <2 x i32> <i32 5, i32 13>
  %tmp882 = shufflevector <16 x double> %tmp875, <16 x double> undef, <2 x i32> <i32 6, i32 14>
  %tmp883 = shufflevector <16 x double> %tmp875, <16 x double> undef, <2 x i32> <i32 7, i32 15>
  %tmp884 = fadd <2 x double> %tmp876, %tmp880
  %tmp885 = fadd <2 x double> %tmp865, %tmp869
  %tmp886 = fsub <2 x double> %tmp876, %tmp880
  %tmp887 = fsub <2 x double> %tmp865, %tmp869
  %tmp888 = fadd <2 x double> %tmp877, %tmp881
  %tmp889 = fadd <2 x double> %tmp866, %tmp870
  %tmp890 = fsub <2 x double> %tmp877, %tmp881
  %tmp891 = fsub <2 x double> %tmp866, %tmp870
  %tmp892 = fadd <2 x double> %tmp878, %tmp882
  %tmp893 = fadd <2 x double> %tmp867, %tmp871
  %tmp894 = fsub <2 x double> %tmp878, %tmp882
  %tmp895 = fsub <2 x double> %tmp867, %tmp871
  %tmp896 = fadd <2 x double> %tmp879, %tmp883
  %tmp897 = fadd <2 x double> %tmp868, %tmp872
  %tmp898 = fsub <2 x double> %tmp879, %tmp883
  %tmp899 = fsub <2 x double> %tmp868, %tmp872
  %tmp900 = fadd <2 x double> %tmp891, %tmp890
  %tmp901 = fmul <2 x double> %tmp900, <double 0x3FE6A09E667F3BCD, double 0x3FE6A09E667F3BCD>
  %tmp902 = fsub <2 x double> %tmp891, %tmp890
  %tmp903 = fmul <2 x double> %tmp902, <double 0x3FE6A09E667F3BCD, double 0x3FE6A09E667F3BCD>
  %tmp904 = fmul <2 x double> %tmp894, zeroinitializer
  %tmp905 = fadd <2 x double> %tmp895, %tmp904
  %tmp906 = fmul <2 x double> %tmp895, zeroinitializer
  %tmp907 = fsub <2 x double> %tmp906, %tmp894
  %tmp908 = fsub <2 x double> %tmp899, %tmp898
  %tmp909 = fmul <2 x double> %tmp908, <double 0x3FE6A09E667F3BCD, double 0x3FE6A09E667F3BCD>
  %tmp910 = fsub <2 x double> <double -0.000000e+00, double -0.000000e+00>, %tmp899
  %tmp911 = fsub <2 x double> %tmp910, %tmp898
  %tmp912 = fmul <2 x double> %tmp911, <double 0x3FE6A09E667F3BCD, double 0x3FE6A09E667F3BCD>
  %tmp913 = fadd <2 x double> %tmp884, %tmp892
  %tmp914 = fadd <2 x double> %tmp885, %tmp893
  %tmp915 = fsub <2 x double> %tmp884, %tmp892
  %tmp916 = fsub <2 x double> %tmp885, %tmp893
  %tmp917 = fadd <2 x double> %tmp888, %tmp896
  %tmp918 = fadd <2 x double> %tmp889, %tmp897
  %tmp919 = fsub <2 x double> %tmp888, %tmp896
  %tmp920 = fsub <2 x double> %tmp889, %tmp897
  %tmp921 = fmul <2 x double> %tmp919, zeroinitializer
  %tmp922 = fadd <2 x double> %tmp920, %tmp921
  %tmp923 = fsub <2 x double> <double -0.000000e+00, double -0.000000e+00>, %tmp919
  %tmp924 = fmul <2 x double> %tmp920, zeroinitializer
  %tmp925 = fsub <2 x double> %tmp923, %tmp924
  %tmp926 = fadd <2 x double> %tmp913, %tmp917
  %tmp927 = fadd <2 x double> %tmp914, %tmp918
  %tmp928 = fsub <2 x double> %tmp913, %tmp917
  %tmp929 = fsub <2 x double> %tmp914, %tmp918
  %tmp930 = fadd <2 x double> %tmp915, %tmp922
  %tmp931 = fadd <2 x double> %tmp916, %tmp925
  %tmp932 = fsub <2 x double> %tmp915, %tmp922
  %tmp933 = fsub <2 x double> %tmp916, %tmp925
  %tmp934 = fadd <2 x double> %tmp886, %tmp905
  %tmp935 = fadd <2 x double> %tmp887, %tmp907
  %tmp936 = fsub <2 x double> %tmp886, %tmp905
  %tmp937 = fsub <2 x double> %tmp887, %tmp907
  %tmp938 = fadd <2 x double> %tmp901, %tmp909
  %tmp939 = fadd <2 x double> %tmp903, %tmp912
  %tmp940 = fsub <2 x double> %tmp901, %tmp909
  %tmp941 = fsub <2 x double> %tmp903, %tmp912
  %tmp942 = fmul <2 x double> %tmp940, zeroinitializer
  %tmp943 = fadd <2 x double> %tmp941, %tmp942
  %tmp944 = fsub <2 x double> <double -0.000000e+00, double -0.000000e+00>, %tmp940
  %tmp945 = fmul <2 x double> %tmp941, zeroinitializer
  %tmp946 = fsub <2 x double> %tmp944, %tmp945
  %tmp947 = fadd <2 x double> %tmp934, %tmp938
  %tmp948 = fadd <2 x double> %tmp935, %tmp939
  %tmp949 = fsub <2 x double> %tmp934, %tmp938
  %tmp950 = fsub <2 x double> %tmp935, %tmp939
  %tmp951 = fadd <2 x double> %tmp936, %tmp943
  %tmp952 = fadd <2 x double> %tmp937, %tmp946
  %tmp953 = fsub <2 x double> %tmp936, %tmp943
  %tmp954 = fsub <2 x double> %tmp937, %tmp946
  %tmp955 = getelementptr inbounds double, double* %arg, i64 %tmp860
  %tmp956 = bitcast double* %tmp955 to <2 x double>*
  store <2 x double> %tmp926, <2 x double>* %tmp956, align 8, !tbaa !2, !alias.scope !11, !noalias !14
  %tmp957 = add nuw nsw i64 %tmp860, 64
  %tmp958 = getelementptr inbounds double, double* %arg, i64 %tmp957
  %tmp959 = bitcast double* %tmp958 to <2 x double>*
  store <2 x double> %tmp947, <2 x double>* %tmp959, align 8, !tbaa !2, !alias.scope !11, !noalias !14
  %tmp960 = add nuw nsw i64 %tmp860, 128
  %tmp961 = getelementptr inbounds double, double* %arg, i64 %tmp960
  %tmp962 = bitcast double* %tmp961 to <2 x double>*
  store <2 x double> %tmp930, <2 x double>* %tmp962, align 8, !tbaa !2, !alias.scope !11, !noalias !14
  %tmp963 = add nuw nsw i64 %tmp860, 192
  %tmp964 = getelementptr inbounds double, double* %arg, i64 %tmp963
  %tmp965 = bitcast double* %tmp964 to <2 x double>*
  store <2 x double> %tmp951, <2 x double>* %tmp965, align 8, !tbaa !2, !alias.scope !11, !noalias !14
  %tmp966 = add nuw nsw i64 %tmp860, 256
  %tmp967 = getelementptr inbounds double, double* %arg, i64 %tmp966
  %tmp968 = bitcast double* %tmp967 to <2 x double>*
  store <2 x double> %tmp928, <2 x double>* %tmp968, align 8, !tbaa !2, !alias.scope !11, !noalias !14
  %tmp969 = add nuw nsw i64 %tmp860, 320
  %tmp970 = getelementptr inbounds double, double* %arg, i64 %tmp969
  %tmp971 = bitcast double* %tmp970 to <2 x double>*
  store <2 x double> %tmp949, <2 x double>* %tmp971, align 8, !tbaa !2, !alias.scope !11, !noalias !14
  %tmp972 = add nuw nsw i64 %tmp860, 384
  %tmp973 = getelementptr inbounds double, double* %arg, i64 %tmp972
  %tmp974 = bitcast double* %tmp973 to <2 x double>*
  store <2 x double> %tmp932, <2 x double>* %tmp974, align 8, !tbaa !2, !alias.scope !11, !noalias !14
  %tmp975 = add nuw nsw i64 %tmp860, 448
  %tmp976 = getelementptr inbounds double, double* %arg, i64 %tmp975
  %tmp977 = bitcast double* %tmp976 to <2 x double>*
  store <2 x double> %tmp953, <2 x double>* %tmp977, align 8, !tbaa !2, !alias.scope !11, !noalias !14
  %tmp978 = getelementptr inbounds double, double* %arg1, i64 %tmp860
  %tmp979 = bitcast double* %tmp978 to <2 x double>*
  store <2 x double> %tmp927, <2 x double>* %tmp979, align 8, !tbaa !2, !alias.scope !14
  %tmp980 = getelementptr inbounds double, double* %arg1, i64 %tmp957
  %tmp981 = bitcast double* %tmp980 to <2 x double>*
  store <2 x double> %tmp948, <2 x double>* %tmp981, align 8, !tbaa !2, !alias.scope !14
  %tmp982 = getelementptr inbounds double, double* %arg1, i64 %tmp960
  %tmp983 = bitcast double* %tmp982 to <2 x double>*
  store <2 x double> %tmp931, <2 x double>* %tmp983, align 8, !tbaa !2, !alias.scope !14
  %tmp984 = getelementptr inbounds double, double* %arg1, i64 %tmp963
  %tmp985 = bitcast double* %tmp984 to <2 x double>*
  store <2 x double> %tmp952, <2 x double>* %tmp985, align 8, !tbaa !2, !alias.scope !14
  %tmp986 = getelementptr inbounds double, double* %arg1, i64 %tmp966
  %tmp987 = bitcast double* %tmp986 to <2 x double>*
  store <2 x double> %tmp929, <2 x double>* %tmp987, align 8, !tbaa !2, !alias.scope !14
  %tmp988 = getelementptr inbounds double, double* %arg1, i64 %tmp969
  %tmp989 = bitcast double* %tmp988 to <2 x double>*
  store <2 x double> %tmp950, <2 x double>* %tmp989, align 8, !tbaa !2, !alias.scope !14
  %tmp990 = getelementptr inbounds double, double* %arg1, i64 %tmp972
  %tmp991 = bitcast double* %tmp990 to <2 x double>*
  store <2 x double> %tmp933, <2 x double>* %tmp991, align 8, !tbaa !2, !alias.scope !14
  %tmp992 = getelementptr inbounds double, double* %arg1, i64 %tmp975
  %tmp993 = bitcast double* %tmp992 to <2 x double>*
  store <2 x double> %tmp954, <2 x double>* %tmp993, align 8, !tbaa !2, !alias.scope !14
  %tmp994 = add i64 %tmp860, 2
  %tmp995 = icmp eq i64 %tmp994, 64
  br i1 %tmp995, label %bb1134, label %bb859, !llvm.loop !16

bb996:                                            ; preds = %bb996, %bb858
  %tmp997 = phi i64 [ %tmp1132, %bb996 ], [ 0, %bb858 ]
  %tmp998 = shl nsw i64 %tmp997, 3
  %tmp999 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp998
  %tmp1000 = load double, double* %tmp999, align 16, !tbaa !2
  %tmp1001 = or i64 %tmp998, 1
  %tmp1002 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp1001
  %tmp1003 = load double, double* %tmp1002, align 8, !tbaa !2
  %tmp1004 = or i64 %tmp998, 2
  %tmp1005 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp1004
  %tmp1006 = load double, double* %tmp1005, align 16, !tbaa !2
  %tmp1007 = or i64 %tmp998, 3
  %tmp1008 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp1007
  %tmp1009 = load double, double* %tmp1008, align 8, !tbaa !2
  %tmp1010 = or i64 %tmp998, 4
  %tmp1011 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp1010
  %tmp1012 = load double, double* %tmp1011, align 16, !tbaa !2
  %tmp1013 = or i64 %tmp998, 5
  %tmp1014 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp1013
  %tmp1015 = load double, double* %tmp1014, align 8, !tbaa !2
  %tmp1016 = or i64 %tmp998, 6
  %tmp1017 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp1016
  %tmp1018 = load double, double* %tmp1017, align 16, !tbaa !2
  %tmp1019 = or i64 %tmp998, 7
  %tmp1020 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp1019
  %tmp1021 = load double, double* %tmp1020, align 8, !tbaa !2
  %tmp1022 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp998
  %tmp1023 = load double, double* %tmp1022, align 16, !tbaa !2
  %tmp1024 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp1001
  %tmp1025 = load double, double* %tmp1024, align 8, !tbaa !2
  %tmp1026 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp1004
  %tmp1027 = load double, double* %tmp1026, align 16, !tbaa !2
  %tmp1028 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp1007
  %tmp1029 = load double, double* %tmp1028, align 8, !tbaa !2
  %tmp1030 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp1010
  %tmp1031 = load double, double* %tmp1030, align 16, !tbaa !2
  %tmp1032 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp1013
  %tmp1033 = load double, double* %tmp1032, align 8, !tbaa !2
  %tmp1034 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp1016
  %tmp1035 = load double, double* %tmp1034, align 16, !tbaa !2
  %tmp1036 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp1019
  %tmp1037 = load double, double* %tmp1036, align 8, !tbaa !2
  %tmp1038 = fadd double %tmp1023, %tmp1031
  %tmp1039 = fadd double %tmp1000, %tmp1012
  %tmp1040 = fsub double %tmp1023, %tmp1031
  %tmp1041 = fsub double %tmp1000, %tmp1012
  %tmp1042 = fadd double %tmp1025, %tmp1033
  %tmp1043 = fadd double %tmp1003, %tmp1015
  %tmp1044 = fsub double %tmp1025, %tmp1033
  %tmp1045 = fsub double %tmp1003, %tmp1015
  %tmp1046 = fadd double %tmp1027, %tmp1035
  %tmp1047 = fadd double %tmp1006, %tmp1018
  %tmp1048 = fsub double %tmp1027, %tmp1035
  %tmp1049 = fsub double %tmp1006, %tmp1018
  %tmp1050 = fadd double %tmp1029, %tmp1037
  %tmp1051 = fadd double %tmp1009, %tmp1021
  %tmp1052 = fsub double %tmp1029, %tmp1037
  %tmp1053 = fsub double %tmp1009, %tmp1021
  %tmp1054 = fadd double %tmp1045, %tmp1044
  %tmp1055 = fmul double %tmp1054, 0x3FE6A09E667F3BCD
  %tmp1056 = fsub double %tmp1045, %tmp1044
  %tmp1057 = fmul double %tmp1056, 0x3FE6A09E667F3BCD
  %tmp1058 = fmul double %tmp1048, 0.000000e+00
  %tmp1059 = fadd double %tmp1049, %tmp1058
  %tmp1060 = fmul double %tmp1049, 0.000000e+00
  %tmp1061 = fsub double %tmp1060, %tmp1048
  %tmp1062 = fsub double %tmp1053, %tmp1052
  %tmp1063 = fmul double %tmp1062, 0x3FE6A09E667F3BCD
  %tmp1064 = fsub double -0.000000e+00, %tmp1053
  %tmp1065 = fsub double %tmp1064, %tmp1052
  %tmp1066 = fmul double %tmp1065, 0x3FE6A09E667F3BCD
  %tmp1067 = fadd double %tmp1038, %tmp1046
  %tmp1068 = fadd double %tmp1039, %tmp1047
  %tmp1069 = fsub double %tmp1038, %tmp1046
  %tmp1070 = fsub double %tmp1039, %tmp1047
  %tmp1071 = fadd double %tmp1042, %tmp1050
  %tmp1072 = fadd double %tmp1043, %tmp1051
  %tmp1073 = fsub double %tmp1042, %tmp1050
  %tmp1074 = fsub double %tmp1043, %tmp1051
  %tmp1075 = fmul double %tmp1073, 0.000000e+00
  %tmp1076 = fadd double %tmp1074, %tmp1075
  %tmp1077 = fsub double -0.000000e+00, %tmp1073
  %tmp1078 = fmul double %tmp1074, 0.000000e+00
  %tmp1079 = fsub double %tmp1077, %tmp1078
  %tmp1080 = fadd double %tmp1067, %tmp1071
  %tmp1081 = fadd double %tmp1068, %tmp1072
  %tmp1082 = fsub double %tmp1067, %tmp1071
  %tmp1083 = fsub double %tmp1068, %tmp1072
  %tmp1084 = fadd double %tmp1069, %tmp1076
  %tmp1085 = fadd double %tmp1070, %tmp1079
  %tmp1086 = fsub double %tmp1069, %tmp1076
  %tmp1087 = fsub double %tmp1070, %tmp1079
  %tmp1088 = fadd double %tmp1040, %tmp1059
  %tmp1089 = fadd double %tmp1041, %tmp1061
  %tmp1090 = fsub double %tmp1040, %tmp1059
  %tmp1091 = fsub double %tmp1041, %tmp1061
  %tmp1092 = fadd double %tmp1055, %tmp1063
  %tmp1093 = fadd double %tmp1057, %tmp1066
  %tmp1094 = fsub double %tmp1055, %tmp1063
  %tmp1095 = fsub double %tmp1057, %tmp1066
  %tmp1096 = fmul double %tmp1094, 0.000000e+00
  %tmp1097 = fadd double %tmp1095, %tmp1096
  %tmp1098 = fsub double -0.000000e+00, %tmp1094
  %tmp1099 = fmul double %tmp1095, 0.000000e+00
  %tmp1100 = fsub double %tmp1098, %tmp1099
  %tmp1101 = fadd double %tmp1088, %tmp1092
  %tmp1102 = fadd double %tmp1089, %tmp1093
  %tmp1103 = fsub double %tmp1088, %tmp1092
  %tmp1104 = fsub double %tmp1089, %tmp1093
  %tmp1105 = fadd double %tmp1090, %tmp1097
  %tmp1106 = fadd double %tmp1091, %tmp1100
  %tmp1107 = fsub double %tmp1090, %tmp1097
  %tmp1108 = fsub double %tmp1091, %tmp1100
  %tmp1109 = getelementptr inbounds double, double* %arg, i64 %tmp997
  store double %tmp1080, double* %tmp1109, align 8, !tbaa !2
  %tmp1110 = add nuw nsw i64 %tmp997, 64
  %tmp1111 = getelementptr inbounds double, double* %arg, i64 %tmp1110
  store double %tmp1101, double* %tmp1111, align 8, !tbaa !2
  %tmp1112 = add nuw nsw i64 %tmp997, 128
  %tmp1113 = getelementptr inbounds double, double* %arg, i64 %tmp1112
  store double %tmp1084, double* %tmp1113, align 8, !tbaa !2
  %tmp1114 = add nuw nsw i64 %tmp997, 192
  %tmp1115 = getelementptr inbounds double, double* %arg, i64 %tmp1114
  store double %tmp1105, double* %tmp1115, align 8, !tbaa !2
  %tmp1116 = add nuw nsw i64 %tmp997, 256
  %tmp1117 = getelementptr inbounds double, double* %arg, i64 %tmp1116
  store double %tmp1082, double* %tmp1117, align 8, !tbaa !2
  %tmp1118 = add nuw nsw i64 %tmp997, 320
  %tmp1119 = getelementptr inbounds double, double* %arg, i64 %tmp1118
  store double %tmp1103, double* %tmp1119, align 8, !tbaa !2
  %tmp1120 = add nuw nsw i64 %tmp997, 384
  %tmp1121 = getelementptr inbounds double, double* %arg, i64 %tmp1120
  store double %tmp1086, double* %tmp1121, align 8, !tbaa !2
  %tmp1122 = add nuw nsw i64 %tmp997, 448
  %tmp1123 = getelementptr inbounds double, double* %arg, i64 %tmp1122
  store double %tmp1107, double* %tmp1123, align 8, !tbaa !2
  %tmp1124 = getelementptr inbounds double, double* %arg1, i64 %tmp997
  store double %tmp1081, double* %tmp1124, align 8, !tbaa !2
  %tmp1125 = getelementptr inbounds double, double* %arg1, i64 %tmp1110
  store double %tmp1102, double* %tmp1125, align 8, !tbaa !2
  %tmp1126 = getelementptr inbounds double, double* %arg1, i64 %tmp1112
  store double %tmp1085, double* %tmp1126, align 8, !tbaa !2
  %tmp1127 = getelementptr inbounds double, double* %arg1, i64 %tmp1114
  store double %tmp1106, double* %tmp1127, align 8, !tbaa !2
  %tmp1128 = getelementptr inbounds double, double* %arg1, i64 %tmp1116
  store double %tmp1083, double* %tmp1128, align 8, !tbaa !2
  %tmp1129 = getelementptr inbounds double, double* %arg1, i64 %tmp1118
  store double %tmp1104, double* %tmp1129, align 8, !tbaa !2
  %tmp1130 = getelementptr inbounds double, double* %arg1, i64 %tmp1120
  store double %tmp1087, double* %tmp1130, align 8, !tbaa !2
  %tmp1131 = getelementptr inbounds double, double* %arg1, i64 %tmp1122
  store double %tmp1108, double* %tmp1131, align 8, !tbaa !2
  %tmp1132 = add nuw nsw i64 %tmp997, 1
  %tmp1133 = icmp eq i64 %tmp1132, 64
  br i1 %tmp1133, label %bb1134, label %bb996, !llvm.loop !17

bb1134:                                           ; preds = %bb996, %bb859
  call void @llvm.lifetime.end.p0i8(i64 4608, i8* nonnull %tmp6) #10
  call void @llvm.lifetime.end.p0i8(i64 4096, i8* nonnull %tmp5) #10
  call void @llvm.lifetime.end.p0i8(i64 4096, i8* nonnull %tmp4) #10
  ret void
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64, i8* nocapture) #4

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64, i8* nocapture) #4

; Function Attrs: nounwind uwtable
define internal dso_local void @fft(double* nocapture %arg, double* nocapture %arg1) #0 {
bb:
  tail call void @fft1D_512(double* %arg, double* %arg1)
  tail call void @fft1D_512(double* %arg, double* %arg1)
  tail call void @fft1D_512(double* %arg, double* %arg1)
  ret void
}

; Function Attrs: nounwind uwtable
define internal dso_local void @run_benchmark(i8* %arg) #0 {
bb:
  %tmp = bitcast i8* %arg to double*
  %tmp1 = getelementptr inbounds i8, i8* %arg, i64 4096
  %tmp2 = bitcast i8* %tmp1 to double*
  %tmp3 = tail call i32 (double*, double*, ...) bitcast (void (double*, double*)* @fft to i32 (double*, double*, ...)*)(double* %tmp, double* nonnull %tmp2) #10
  ret void
}

; Function Attrs: nounwind uwtable
define internal dso_local void @input_to_data(i32 %arg, i8* %arg1) #0 {
bb:
  %tmp = tail call i8* @readfile(i32 %arg) #10
  %tmp2 = tail call i8* @find_section_start(i8* %tmp, i32 1) #10
  %tmp3 = bitcast i8* %arg1 to double*
  %tmp4 = tail call i32 @parse_double_array(i8* %tmp2, double* %tmp3, i32 512) #10
  %tmp5 = tail call i8* @find_section_start(i8* %tmp, i32 2) #10
  %tmp6 = getelementptr inbounds i8, i8* %arg1, i64 4096
  %tmp7 = bitcast i8* %tmp6 to double*
  %tmp8 = tail call i32 @parse_double_array(i8* %tmp5, double* nonnull %tmp7, i32 512) #10
  tail call void @free(i8* %tmp) #10
  ret void
}

; Function Attrs: nounwind
declare void @free(i8* nocapture) local_unnamed_addr #1

; Function Attrs: nounwind uwtable
define internal dso_local void @data_to_input(i32 %arg, i8* %arg1) #0 {
bb:
  %tmp = tail call i32 @write_section_header(i32 %arg) #10
  %tmp2 = bitcast i8* %arg1 to double*
  %tmp3 = tail call i32 @write_double_array(i32 %arg, double* %tmp2, i32 512) #10
  %tmp4 = tail call i32 @write_section_header(i32 %arg) #10
  %tmp5 = getelementptr inbounds i8, i8* %arg1, i64 4096
  %tmp6 = bitcast i8* %tmp5 to double*
  %tmp7 = tail call i32 @write_double_array(i32 %arg, double* nonnull %tmp6, i32 512) #10
  ret void
}

; Function Attrs: nounwind uwtable
define internal dso_local void @output_to_data(i32 %arg, i8* %arg1) #0 {
bb:
  %tmp = tail call i8* @readfile(i32 %arg) #10
  %tmp2 = tail call i8* @find_section_start(i8* %tmp, i32 1) #10
  %tmp3 = bitcast i8* %arg1 to double*
  %tmp4 = tail call i32 @parse_double_array(i8* %tmp2, double* %tmp3, i32 512) #10
  %tmp5 = tail call i8* @find_section_start(i8* %tmp, i32 2) #10
  %tmp6 = getelementptr inbounds i8, i8* %arg1, i64 4096
  %tmp7 = bitcast i8* %tmp6 to double*
  %tmp8 = tail call i32 @parse_double_array(i8* %tmp5, double* nonnull %tmp7, i32 512) #10
  tail call void @free(i8* %tmp) #10
  ret void
}

; Function Attrs: nounwind uwtable
define internal dso_local void @data_to_output(i32 %arg, i8* %arg1) #0 {
bb:
  %tmp = tail call i32 @write_section_header(i32 %arg) #10
  %tmp2 = bitcast i8* %arg1 to double*
  %tmp3 = tail call i32 @write_double_array(i32 %arg, double* %tmp2, i32 512) #10
  %tmp4 = tail call i32 @write_section_header(i32 %arg) #10
  %tmp5 = getelementptr inbounds i8, i8* %arg1, i64 4096
  %tmp6 = bitcast i8* %tmp5 to double*
  %tmp7 = tail call i32 @write_double_array(i32 %arg, double* nonnull %tmp6, i32 512) #10
  ret void
}

; Function Attrs: norecurse nounwind readonly uwtable
define internal dso_local i32 @check_data(i8* nocapture readonly %arg, i8* nocapture readonly %arg1) #5 {
bb:
  %tmp = bitcast i8* %arg to [512 x double]*
  %tmp2 = bitcast i8* %arg1 to [512 x double]*
  %tmp3 = getelementptr inbounds i8, i8* %arg, i64 4096
  %tmp4 = bitcast i8* %tmp3 to [512 x double]*
  %tmp5 = getelementptr inbounds i8, i8* %arg1, i64 4096
  %tmp6 = bitcast i8* %tmp5 to [512 x double]*
  br label %bb7

bb7:                                              ; preds = %bb7, %bb
  %tmp8 = phi i64 [ 0, %bb ], [ %tmp59, %bb7 ]
  %tmp9 = phi <2 x i32> [ zeroinitializer, %bb ], [ %tmp57, %bb7 ]
  %tmp10 = phi <2 x i32> [ zeroinitializer, %bb ], [ %tmp58, %bb7 ]
  %tmp11 = getelementptr inbounds [512 x double], [512 x double]* %tmp, i64 0, i64 %tmp8
  %tmp12 = bitcast double* %tmp11 to <2 x double>*
  %tmp13 = load <2 x double>, <2 x double>* %tmp12, align 8, !tbaa !2
  %tmp14 = getelementptr double, double* %tmp11, i64 2
  %tmp15 = bitcast double* %tmp14 to <2 x double>*
  %tmp16 = load <2 x double>, <2 x double>* %tmp15, align 8, !tbaa !2
  %tmp17 = getelementptr inbounds [512 x double], [512 x double]* %tmp2, i64 0, i64 %tmp8
  %tmp18 = bitcast double* %tmp17 to <2 x double>*
  %tmp19 = load <2 x double>, <2 x double>* %tmp18, align 8, !tbaa !2
  %tmp20 = getelementptr double, double* %tmp17, i64 2
  %tmp21 = bitcast double* %tmp20 to <2 x double>*
  %tmp22 = load <2 x double>, <2 x double>* %tmp21, align 8, !tbaa !2
  %tmp23 = fsub <2 x double> %tmp13, %tmp19
  %tmp24 = fsub <2 x double> %tmp16, %tmp22
  %tmp25 = getelementptr inbounds [512 x double], [512 x double]* %tmp4, i64 0, i64 %tmp8
  %tmp26 = bitcast double* %tmp25 to <2 x double>*
  %tmp27 = load <2 x double>, <2 x double>* %tmp26, align 8, !tbaa !2
  %tmp28 = getelementptr double, double* %tmp25, i64 2
  %tmp29 = bitcast double* %tmp28 to <2 x double>*
  %tmp30 = load <2 x double>, <2 x double>* %tmp29, align 8, !tbaa !2
  %tmp31 = getelementptr inbounds [512 x double], [512 x double]* %tmp6, i64 0, i64 %tmp8
  %tmp32 = bitcast double* %tmp31 to <2 x double>*
  %tmp33 = load <2 x double>, <2 x double>* %tmp32, align 8, !tbaa !2
  %tmp34 = getelementptr double, double* %tmp31, i64 2
  %tmp35 = bitcast double* %tmp34 to <2 x double>*
  %tmp36 = load <2 x double>, <2 x double>* %tmp35, align 8, !tbaa !2
  %tmp37 = fsub <2 x double> %tmp27, %tmp33
  %tmp38 = fsub <2 x double> %tmp30, %tmp36
  %tmp39 = fcmp olt <2 x double> %tmp23, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp40 = fcmp olt <2 x double> %tmp24, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp41 = fcmp ogt <2 x double> %tmp23, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp42 = fcmp ogt <2 x double> %tmp24, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp43 = or <2 x i1> %tmp39, %tmp41
  %tmp44 = or <2 x i1> %tmp40, %tmp42
  %tmp45 = zext <2 x i1> %tmp43 to <2 x i32>
  %tmp46 = zext <2 x i1> %tmp44 to <2 x i32>
  %tmp47 = or <2 x i32> %tmp9, %tmp45
  %tmp48 = or <2 x i32> %tmp10, %tmp46
  %tmp49 = fcmp olt <2 x double> %tmp37, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp50 = fcmp olt <2 x double> %tmp38, <double 0xBEB0C6F7A0B5ED8D, double 0xBEB0C6F7A0B5ED8D>
  %tmp51 = fcmp ogt <2 x double> %tmp37, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp52 = fcmp ogt <2 x double> %tmp38, <double 0x3EB0C6F7A0B5ED8D, double 0x3EB0C6F7A0B5ED8D>
  %tmp53 = or <2 x i1> %tmp49, %tmp51
  %tmp54 = or <2 x i1> %tmp50, %tmp52
  %tmp55 = zext <2 x i1> %tmp53 to <2 x i32>
  %tmp56 = zext <2 x i1> %tmp54 to <2 x i32>
  %tmp57 = or <2 x i32> %tmp47, %tmp55
  %tmp58 = or <2 x i32> %tmp48, %tmp56
  %tmp59 = add i64 %tmp8, 4
  %tmp60 = icmp eq i64 %tmp59, 512
  br i1 %tmp60, label %bb61, label %bb7, !llvm.loop !18

bb61:                                             ; preds = %bb7
  %tmp62 = or <2 x i32> %tmp58, %tmp57
  %tmp63 = shufflevector <2 x i32> %tmp62, <2 x i32> undef, <2 x i32> <i32 1, i32 undef>
  %tmp64 = or <2 x i32> %tmp62, %tmp63
  %tmp65 = extractelement <2 x i32> %tmp64, i32 0
  %tmp66 = icmp eq i32 %tmp65, 0
  %tmp67 = zext i1 %tmp66 to i32
  ret i32 %tmp67
}

; Function Attrs: nounwind uwtable
define internal dso_local noalias i8* @readfile(i32 %arg) #0 {
bb:
  %tmp = alloca %struct.stat, align 8
  %tmp1 = bitcast %struct.stat* %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 144, i8* nonnull %tmp1) #10
  %tmp2 = icmp sgt i32 %arg, 1
  br i1 %tmp2, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 40, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #11
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = call i32 @__fxstat(i32 1, i32 %arg, %struct.stat* nonnull %tmp) #10
  %tmp6 = icmp eq i32 %tmp5, 0
  br i1 %tmp6, label %bb8, label %bb7

bb7:                                              ; preds = %bb4
  call void @__assert_fail(i8* getelementptr inbounds ([51 x i8], [51 x i8]* @.str.4, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 41, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #11
  unreachable

bb8:                                              ; preds = %bb4
  %tmp9 = getelementptr inbounds %struct.stat, %struct.stat* %tmp, i64 0, i32 8
  %tmp10 = load i64, i64* %tmp9, align 8, !tbaa !19
  %tmp11 = icmp sgt i64 %tmp10, 0
  br i1 %tmp11, label %bb13, label %bb12

bb12:                                             ; preds = %bb8
  call void @__assert_fail(i8* getelementptr inbounds ([25 x i8], [25 x i8]* @.str.6, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 43, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #11
  unreachable

bb13:                                             ; preds = %bb8
  %tmp14 = add nsw i64 %tmp10, 1
  %tmp15 = call noalias i8* @malloc(i64 %tmp14) #10
  br label %bb18

bb16:                                             ; preds = %bb18
  %tmp17 = icmp sgt i64 %tmp10, %tmp24
  br i1 %tmp17, label %bb18, label %bb26

bb18:                                             ; preds = %bb16, %bb13
  %tmp19 = phi i64 [ 0, %bb13 ], [ %tmp24, %bb16 ]
  %tmp20 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp19
  %tmp21 = sub nsw i64 %tmp10, %tmp19
  %tmp22 = call i64 @read(i32 %arg, i8* %tmp20, i64 %tmp21) #10
  %tmp23 = icmp sgt i64 %tmp22, -1
  %tmp24 = add nsw i64 %tmp22, %tmp19
  br i1 %tmp23, label %bb16, label %bb25

bb25:                                             ; preds = %bb18
  call void @__assert_fail(i8* getelementptr inbounds ([29 x i8], [29 x i8]* @.str.8, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 48, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #11
  unreachable

bb26:                                             ; preds = %bb16
  %tmp27 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp10
  store i8 0, i8* %tmp27, align 1, !tbaa !24
  %tmp28 = call i32 @close(i32 %arg) #10
  call void @llvm.lifetime.end.p0i8(i64 144, i8* nonnull %tmp1) #10
  ret i8* %tmp15
}

; Function Attrs: noreturn nounwind
declare void @__assert_fail(i8*, i8*, i32, i8*) local_unnamed_addr #6

; Function Attrs: nounwind
declare i32 @__fxstat(i32, i32, %struct.stat*) local_unnamed_addr #1

; Function Attrs: nounwind
declare noalias i8* @malloc(i64) local_unnamed_addr #1

declare i64 @read(i32, i8* nocapture, i64) local_unnamed_addr #7

declare i32 @close(i32) local_unnamed_addr #7

; Function Attrs: nounwind uwtable
define internal dso_local i8* @find_section_start(i8* readonly %arg, i32 %arg1) #0 {
bb:
  %tmp = icmp sgt i32 %arg1, -1
  br i1 %tmp, label %bb3, label %bb2

bb2:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([33 x i8], [33 x i8]* @.str.10, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 59, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.find_section_start, i64 0, i64 0)) #11
  unreachable

bb3:                                              ; preds = %bb
  %tmp4 = icmp eq i32 %arg1, 0
  br i1 %tmp4, label %bb34, label %bb5

bb5:                                              ; preds = %bb3
  %tmp6 = load i8, i8* %arg, align 1, !tbaa !24
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
  %tmp13 = load i8, i8* %tmp12, align 1, !tbaa !24
  br label %bb24

bb14:                                             ; preds = %bb7
  %tmp15 = getelementptr inbounds i8, i8* %tmp10, i64 1
  %tmp16 = load i8, i8* %tmp15, align 1, !tbaa !24
  %tmp17 = icmp eq i8 %tmp16, 37
  br i1 %tmp17, label %bb18, label %bb24

bb18:                                             ; preds = %bb14
  %tmp19 = getelementptr inbounds i8, i8* %tmp10, i64 2
  %tmp20 = load i8, i8* %tmp19, align 1, !tbaa !24
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
define internal dso_local i32 @parse_string(i8* readonly %arg, i8* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp eq i8* %arg, null
  br i1 %tmp, label %bb3, label %bb4

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 79, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.parse_string, i64 0, i64 0)) #11
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp slt i32 %arg2, 0
  br i1 %tmp5, label %bb8, label %bb6

bb6:                                              ; preds = %bb4
  %tmp7 = sext i32 %arg2 to i64
  tail call void @llvm.memcpy.p0i8.p0i8.i64(i8* %arg1, i8* nonnull %arg, i64 %tmp7, i32 1, i1 false)
  br label %bb47

bb8:                                              ; preds = %bb4
  %tmp9 = load i8, i8* %arg, align 1, !tbaa !24
  %tmp10 = icmp eq i8 %tmp9, 0
  br i1 %tmp10, label %bb43, label %bb11

bb11:                                             ; preds = %bb8
  %tmp12 = getelementptr inbounds i8, i8* %arg, i64 1
  %tmp13 = load i8, i8* %tmp12, align 1, !tbaa !24
  br label %bb17

bb14:                                             ; preds = %bb31
  %tmp15 = load i8, i8* %tmp24, align 1, !tbaa !24
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
  %tmp29 = load i8, i8* %tmp28, align 1, !tbaa !24
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
  store i8 0, i8* %tmp46, align 1, !tbaa !24
  br label %bb47

bb47:                                             ; preds = %bb43, %bb6
  ret i32 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture writeonly, i8* nocapture readonly, i64, i32, i1) #4

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_uint8_t_array(i8* %arg, i8* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #10
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 132, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_uint8_t_array, i64 0, i64 0)) #11
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !25
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #10
  %tmp17 = trunc i64 %tmp16 to i8
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !25
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !24
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !25
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #12
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i8, i8* %arg1, i64 %tmp14
  store i8 %tmp17, i8* %tmp26, align 1, !tbaa !24
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #13
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !24
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #13
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !24
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #10
  ret i32 0
}

; Function Attrs: nounwind
declare i8* @strtok(i8*, i8* nocapture readonly) local_unnamed_addr #1

; Function Attrs: nounwind
declare i64 @strtol(i8* readonly, i8** nocapture, i32) local_unnamed_addr #1

; Function Attrs: nounwind
declare i32 @fprintf(%struct._IO_FILE* nocapture, i8* nocapture readonly, ...) local_unnamed_addr #1

; Function Attrs: argmemonly nounwind readonly
declare i64 @strlen(i8* nocapture) local_unnamed_addr #8

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_uint16_t_array(i8* %arg, i16* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #10
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 133, i8* getelementptr inbounds ([50 x i8], [50 x i8]* @__PRETTY_FUNCTION__.parse_uint16_t_array, i64 0, i64 0)) #11
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !25
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #10
  %tmp17 = trunc i64 %tmp16 to i16
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !25
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !24
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !25
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #12
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i16, i16* %arg1, i64 %tmp14
  store i16 %tmp17, i16* %tmp26, align 2, !tbaa !27
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #13
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !24
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #13
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !24
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #10
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_uint32_t_array(i8* %arg, i32* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #10
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 134, i8* getelementptr inbounds ([50 x i8], [50 x i8]* @__PRETTY_FUNCTION__.parse_uint32_t_array, i64 0, i64 0)) #11
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !25
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #10
  %tmp17 = trunc i64 %tmp16 to i32
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !25
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !24
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !25
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #12
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i32, i32* %arg1, i64 %tmp14
  store i32 %tmp17, i32* %tmp26, align 4, !tbaa !29
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #13
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !24
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #13
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !24
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #10
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_uint64_t_array(i8* %arg, i64* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #10
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 135, i8* getelementptr inbounds ([50 x i8], [50 x i8]* @__PRETTY_FUNCTION__.parse_uint64_t_array, i64 0, i64 0)) #11
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !25
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #10
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !25
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !24
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !25
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #12
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds i64, i64* %arg1, i64 %tmp14
  store i64 %tmp16, i64* %tmp25, align 8, !tbaa !30
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #13
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !24
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #13
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !24
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #10
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_int8_t_array(i8* %arg, i8* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #10
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 136, i8* getelementptr inbounds ([46 x i8], [46 x i8]* @__PRETTY_FUNCTION__.parse_int8_t_array, i64 0, i64 0)) #11
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !25
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #10
  %tmp17 = trunc i64 %tmp16 to i8
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !25
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !24
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !25
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #12
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i8, i8* %arg1, i64 %tmp14
  store i8 %tmp17, i8* %tmp26, align 1, !tbaa !24
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #13
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !24
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #13
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !24
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #10
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_int16_t_array(i8* %arg, i16* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #10
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 137, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_int16_t_array, i64 0, i64 0)) #11
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !25
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #10
  %tmp17 = trunc i64 %tmp16 to i16
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !25
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !24
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !25
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #12
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i16, i16* %arg1, i64 %tmp14
  store i16 %tmp17, i16* %tmp26, align 2, !tbaa !27
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #13
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !24
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #13
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !24
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #10
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_int32_t_array(i8* %arg, i32* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #10
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 138, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_int32_t_array, i64 0, i64 0)) #11
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !25
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #10
  %tmp17 = trunc i64 %tmp16 to i32
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !25
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !24
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !25
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #12
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i32, i32* %arg1, i64 %tmp14
  store i32 %tmp17, i32* %tmp26, align 4, !tbaa !29
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #13
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !24
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #13
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !24
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #10
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_int64_t_array(i8* %arg, i64* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #10
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 139, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_int64_t_array, i64 0, i64 0)) #11
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !25
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #10
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !25
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !24
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !25
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #12
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds i64, i64* %arg1, i64 %tmp14
  store i64 %tmp16, i64* %tmp25, align 8, !tbaa !30
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #13
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !24
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #13
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !24
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #10
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_float_array(i8* %arg, float* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #10
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 141, i8* getelementptr inbounds ([44 x i8], [44 x i8]* @__PRETTY_FUNCTION__.parse_float_array, i64 0, i64 0)) #11
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !25
  %tmp16 = call float @strtof(i8* nonnull %tmp15, i8** nonnull %tmp) #10
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !25
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !24
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !25
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #12
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds float, float* %arg1, i64 %tmp14
  store float %tmp16, float* %tmp25, align 4, !tbaa !31
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #13
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !24
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #13
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !24
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #10
  ret i32 0
}

; Function Attrs: nounwind
declare float @strtof(i8* readonly, i8** nocapture) local_unnamed_addr #1

; Function Attrs: nounwind uwtable
define internal dso_local i32 @parse_double_array(i8* %arg, double* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #10
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 142, i8* getelementptr inbounds ([46 x i8], [46 x i8]* @__PRETTY_FUNCTION__.parse_double_array, i64 0, i64 0)) #11
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !25
  %tmp16 = call double @strtod(i8* nonnull %tmp15, i8** nonnull %tmp) #10
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !25
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !24
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !25
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #12
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds double, double* %arg1, i64 %tmp14
  store double %tmp16, double* %tmp25, align 8, !tbaa !2
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #13
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !24
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #10
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #13
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !24
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #10
  ret i32 0
}

; Function Attrs: nounwind
declare double @strtod(i8* readonly, i8** nocapture) local_unnamed_addr #1

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_string(i32 %arg, i8* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 147, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @__PRETTY_FUNCTION__.write_string, i64 0, i64 0)) #11
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp slt i32 %arg2, 0
  br i1 %tmp5, label %bb6, label %bb9

bb6:                                              ; preds = %bb4
  %tmp7 = tail call i64 @strlen(i8* %arg1) #13
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
  %tmp22 = tail call i64 @write(i32 %arg, i8* %tmp19, i64 %tmp21) #10
  %tmp23 = trunc i64 %tmp22 to i32
  %tmp24 = icmp sgt i32 %tmp23, -1
  %tmp25 = add nsw i32 %tmp17, %tmp23
  br i1 %tmp24, label %bb14, label %bb26

bb26:                                             ; preds = %bb16
  tail call void @__assert_fail(i8* getelementptr inbounds ([28 x i8], [28 x i8]* @.str.16, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 154, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @__PRETTY_FUNCTION__.write_string, i64 0, i64 0)) #11
  unreachable

bb27:                                             ; preds = %bb32, %bb13
  %tmp28 = tail call i64 @write(i32 %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0), i64 1) #10
  %tmp29 = trunc i64 %tmp28 to i32
  %tmp30 = icmp sgt i32 %tmp29, -1
  br i1 %tmp30, label %bb32, label %bb31

bb31:                                             ; preds = %bb27
  tail call void @__assert_fail(i8* getelementptr inbounds ([28 x i8], [28 x i8]* @.str.16, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 160, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @__PRETTY_FUNCTION__.write_string, i64 0, i64 0)) #11
  unreachable

bb32:                                             ; preds = %bb27
  %tmp33 = icmp eq i32 %tmp29, 0
  br i1 %tmp33, label %bb27, label %bb34

bb34:                                             ; preds = %bb32
  ret i32 0
}

declare i64 @write(i32, i8* nocapture readonly, i64) local_unnamed_addr #7

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_uint8_t_array(i32 %arg, i8* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 177, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_uint8_t_array, i64 0, i64 0)) #11
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
  %tmp11 = load i8, i8* %tmp10, align 1, !tbaa !24
  %tmp12 = zext i8 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.17, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: inlinehint nounwind uwtable
define internal void @fd_printf(i32 %arg, i8* nocapture readonly %arg1, ...) unnamed_addr #9 {
bb:
  %tmp = alloca [1 x %struct.__va_list_tag], align 16
  %tmp2 = alloca [256 x i8], align 16
  %tmp3 = bitcast [1 x %struct.__va_list_tag]* %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 24, i8* nonnull %tmp3) #10
  %tmp4 = getelementptr inbounds [256 x i8], [256 x i8]* %tmp2, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 256, i8* nonnull %tmp4) #10
  %tmp5 = getelementptr inbounds [1 x %struct.__va_list_tag], [1 x %struct.__va_list_tag]* %tmp, i64 0, i64 0
  call void @llvm.va_start(i8* nonnull %tmp3)
  %tmp6 = call i32 @vsnprintf(i8* nonnull %tmp4, i64 256, i8* %arg1, %struct.__va_list_tag* nonnull %tmp5) #10
  call void @llvm.va_end(i8* nonnull %tmp3)
  %tmp7 = icmp slt i32 %tmp6, 256
  br i1 %tmp7, label %bb9, label %bb8

bb8:                                              ; preds = %bb
  call void @__assert_fail(i8* getelementptr inbounds ([90 x i8], [90 x i8]* @.str.24, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 22, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.fd_printf, i64 0, i64 0)) #11
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
  %tmp20 = call i64 @write(i32 %arg, i8* nonnull %tmp17, i64 %tmp19) #10
  %tmp21 = trunc i64 %tmp20 to i32
  %tmp22 = icmp sgt i32 %tmp21, -1
  %tmp23 = add nsw i32 %tmp15, %tmp21
  br i1 %tmp22, label %bb12, label %bb24

bb24:                                             ; preds = %bb14
  call void @__assert_fail(i8* getelementptr inbounds ([28 x i8], [28 x i8]* @.str.16, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 26, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.fd_printf, i64 0, i64 0)) #11
  unreachable

bb25:                                             ; preds = %bb12, %bb9
  %tmp26 = phi i32 [ 0, %bb9 ], [ %tmp23, %bb12 ]
  %tmp27 = icmp eq i32 %tmp6, %tmp26
  br i1 %tmp27, label %bb29, label %bb28

bb28:                                             ; preds = %bb25
  call void @__assert_fail(i8* getelementptr inbounds ([50 x i8], [50 x i8]* @.str.26, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 29, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.fd_printf, i64 0, i64 0)) #11
  unreachable

bb29:                                             ; preds = %bb25
  call void @llvm.lifetime.end.p0i8(i64 256, i8* nonnull %tmp4) #10
  call void @llvm.lifetime.end.p0i8(i64 24, i8* nonnull %tmp3) #10
  ret void
}

; Function Attrs: nounwind
declare void @llvm.va_start(i8*) #10

; Function Attrs: nounwind
declare i32 @vsnprintf(i8* nocapture, i64, i8* nocapture readonly, %struct.__va_list_tag*) local_unnamed_addr #1

; Function Attrs: nounwind
declare void @llvm.va_end(i8*) #10

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_uint16_t_array(i32 %arg, i16* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 178, i8* getelementptr inbounds ([47 x i8], [47 x i8]* @__PRETTY_FUNCTION__.write_uint16_t_array, i64 0, i64 0)) #11
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
  %tmp11 = load i16, i16* %tmp10, align 2, !tbaa !27
  %tmp12 = zext i16 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.17, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_uint32_t_array(i32 %arg, i32* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 179, i8* getelementptr inbounds ([47 x i8], [47 x i8]* @__PRETTY_FUNCTION__.write_uint32_t_array, i64 0, i64 0)) #11
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
  %tmp11 = load i32, i32* %tmp10, align 4, !tbaa !29
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.17, i64 0, i64 0), i32 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_uint64_t_array(i32 %arg, i64* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 180, i8* getelementptr inbounds ([47 x i8], [47 x i8]* @__PRETTY_FUNCTION__.write_uint64_t_array, i64 0, i64 0)) #11
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
  %tmp11 = load i64, i64* %tmp10, align 8, !tbaa !30
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str.18, i64 0, i64 0), i64 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_int8_t_array(i32 %arg, i8* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 181, i8* getelementptr inbounds ([43 x i8], [43 x i8]* @__PRETTY_FUNCTION__.write_int8_t_array, i64 0, i64 0)) #11
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
  %tmp11 = load i8, i8* %tmp10, align 1, !tbaa !24
  %tmp12 = sext i8 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.19, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_int16_t_array(i32 %arg, i16* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 182, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_int16_t_array, i64 0, i64 0)) #11
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
  %tmp11 = load i16, i16* %tmp10, align 2, !tbaa !27
  %tmp12 = sext i16 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.19, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_int32_t_array(i32 %arg, i32* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 183, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_int32_t_array, i64 0, i64 0)) #11
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
  %tmp11 = load i32, i32* %tmp10, align 4, !tbaa !29
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.19, i64 0, i64 0), i32 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_int64_t_array(i32 %arg, i64* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 184, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_int64_t_array, i64 0, i64 0)) #11
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
  %tmp11 = load i64, i64* %tmp10, align 8, !tbaa !30
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str.20, i64 0, i64 0), i64 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_float_array(i32 %arg, float* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 186, i8* getelementptr inbounds ([41 x i8], [41 x i8]* @__PRETTY_FUNCTION__.write_float_array, i64 0, i64 0)) #11
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
  %tmp11 = load float, float* %tmp10, align 4, !tbaa !31
  %tmp12 = fpext float %tmp11 to double
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.21, i64 0, i64 0), double %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: nounwind uwtable
define internal dso_local i32 @write_double_array(i32 %arg, double* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 187, i8* getelementptr inbounds ([43 x i8], [43 x i8]* @__PRETTY_FUNCTION__.write_double_array, i64 0, i64 0)) #11
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
define internal dso_local i32 @write_section_header(i32 %arg) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb2, label %bb1

bb1:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 190, i8* getelementptr inbounds ([30 x i8], [30 x i8]* @__PRETTY_FUNCTION__.write_section_header, i64 0, i64 0)) #11
  unreachable

bb2:                                              ; preds = %bb
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.22, i64 0, i64 0))
  ret i32 0
}

; Function Attrs: nounwind uwtable
define dso_local i32 @main(i32 %arg, i8** nocapture readonly %arg1) #0 {
bb:
  %tmp = icmp slt i32 %arg, 4
  br i1 %tmp, label %bb3, label %bb2

bb2:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([57 x i8], [57 x i8]* @.str.1.11, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.12, i64 0, i64 0), i32 21, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #11
  unreachable

bb3:                                              ; preds = %bb
  %tmp4 = icmp sgt i32 %arg, 1
  br i1 %tmp4, label %bb5, label %bb8

bb5:                                              ; preds = %bb3
  %tmp6 = getelementptr inbounds i8*, i8** %arg1, i64 1
  %tmp7 = load i8*, i8** %tmp6, align 8, !tbaa !25
  br label %bb8

bb8:                                              ; preds = %bb5, %bb3
  %tmp9 = phi i8* [ %tmp7, %bb5 ], [ getelementptr inbounds ([11 x i8], [11 x i8]* @.str.3, i64 0, i64 0), %bb3 ]
  %tmp10 = load i32, i32* @INPUT_SIZE, align 4, !tbaa !29
  %tmp11 = sext i32 %tmp10 to i64
  %tmp12 = tail call noalias i8* @malloc(i64 %tmp11) #10
  %tmp13 = icmp eq i8* %tmp12, null
  br i1 %tmp13, label %bb14, label %bb15

bb14:                                             ; preds = %bb8
  tail call void @__assert_fail(i8* getelementptr inbounds ([30 x i8], [30 x i8]* @.str.5, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.12, i64 0, i64 0), i32 37, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #11
  unreachable

bb15:                                             ; preds = %bb8
  %tmp16 = tail call i32 (i8*, i32, ...) @open(i8* %tmp9, i32 0) #10
  %tmp17 = icmp sgt i32 %tmp16, 0
  br i1 %tmp17, label %bb19, label %bb18

bb18:                                             ; preds = %bb15
  tail call void @__assert_fail(i8* getelementptr inbounds ([43 x i8], [43 x i8]* @.str.7, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.12, i64 0, i64 0), i32 39, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #11
  unreachable

bb19:                                             ; preds = %bb15
  tail call void @input_to_data(i32 %tmp16, i8* nonnull %tmp12) #10
  tail call void @run_benchmark(i8* nonnull %tmp12) #10
  tail call void @free(i8* nonnull %tmp12) #10
  %tmp20 = tail call i32 @puts(i8* getelementptr inbounds ([9 x i8], [9 x i8]* @str, i64 0, i64 0))
  ret i32 0
}

declare i32 @open(i8* nocapture readonly, i32, ...) local_unnamed_addr #7

; Function Attrs: nounwind
declare i32 @puts(i8* nocapture readonly) local_unnamed_addr #10

attributes #0 = { nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #2 = { norecurse nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { argmemonly nounwind }
attributes #5 = { norecurse nounwind readonly uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #6 = { noreturn nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #7 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #8 = { argmemonly nounwind readonly "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #9 = { inlinehint nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #10 = { nounwind }
attributes #11 = { noreturn nounwind }
attributes #12 = { cold }
attributes #13 = { nounwind readonly }

!llvm.ident = !{!0, !0, !0, !0}
!llvm.module.flags = !{!1}

!0 = !{!"clang version 6.0.0 (git@github.com:seanzw/clang.git bb8d45f8ab88237f1fa0530b8ad9b96bf4a5e6cc) (git@github.com:seanzw/llvm.git 16ebb58ea40d384e8daa4c48d2bf7dd1ccfa5fcd)"}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{!3, !3, i64 0}
!3 = !{!"double", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C/C++ TBAA"}
!6 = distinct !{!6, !7}
!7 = !{!"llvm.loop.isvectorized", i32 1}
!8 = distinct !{!8, !7}
!9 = distinct !{!9, !7}
!10 = distinct !{!10, !7}
!11 = !{!12}
!12 = distinct !{!12, !13}
!13 = distinct !{!13, !"LVerDomain"}
!14 = !{!15}
!15 = distinct !{!15, !13}
!16 = distinct !{!16, !7}
!17 = distinct !{!17, !7}
!18 = distinct !{!18, !7}
!19 = !{!20, !21, i64 48}
!20 = !{!"stat", !21, i64 0, !21, i64 8, !21, i64 16, !22, i64 24, !22, i64 28, !22, i64 32, !22, i64 36, !21, i64 40, !21, i64 48, !21, i64 56, !21, i64 64, !23, i64 72, !23, i64 88, !23, i64 104, !4, i64 120}
!21 = !{!"long", !4, i64 0}
!22 = !{!"int", !4, i64 0}
!23 = !{!"timespec", !21, i64 0, !21, i64 8}
!24 = !{!4, !4, i64 0}
!25 = !{!26, !26, i64 0}
!26 = !{!"any pointer", !4, i64 0}
!27 = !{!28, !28, i64 0}
!28 = !{!"short", !4, i64 0}
!29 = !{!22, !22, i64 0}
!30 = !{!21, !21, i64 0}
!31 = !{!32, !32, i64 0}
!32 = !{!"float", !4, i64 0}
