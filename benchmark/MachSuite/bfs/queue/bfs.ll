; ModuleID = 'bfs.bc'
source_filename = "ld-temp.o"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

%struct._IO_FILE = type { i32, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*, %struct._IO_marker*, %struct._IO_FILE*, i32, i32, i64, i16, i8, [1 x i8], i8*, i64, i8*, i8*, i8*, i8*, i64, i32, [20 x i8] }
%struct._IO_marker = type { %struct._IO_marker*, %struct._IO_FILE*, i32 }
%struct.node_t_struct = type { i64, i64 }
%struct.edge_t_struct = type { i64 }
%struct.stat = type { i64, i64, i64, i32, i32, i32, i32, i64, i64, i64, i64, %struct.node_t_struct, %struct.node_t_struct, %struct.node_t_struct, [3 x i64] }
%struct.__va_list_tag = type { i32, i32, i8*, i8* }

@INPUT_SIZE = internal dso_local global i32 37208, align 4
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

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @bfs_kernel(%struct.node_t_struct* nocapture readonly %arg, %struct.edge_t_struct* nocapture readonly %arg1, i64 %arg2, i8* nocapture %arg3, i64* nocapture %arg4) #0 {
bb:
  %tmp = alloca [256 x i64], align 16
  %tmp5 = bitcast [256 x i64]* %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 2048, i8* nonnull %tmp5) #7
  %tmp6 = getelementptr inbounds i8, i8* %arg3, i64 %arg2
  %tmp7 = getelementptr inbounds [256 x i64], [256 x i64]* %tmp, i64 0, i64 0
  call void @llvm.memset.p0i8.i64(i8* %arg3, i8 127, i64 256, i32 1, i1 false)
  %tmp8 = getelementptr inbounds i64, i64* %arg4, i64 1
  %tmp9 = bitcast i64* %tmp8 to i8*
  call void @llvm.memset.p0i8.i64(i8* %tmp9, i8 0, i64 72, i32 8, i1 false)
  store i8 0, i8* %tmp6, align 1, !tbaa !2
  store i64 1, i64* %arg4, align 8, !tbaa !5
  store i64 %arg2, i64* %tmp7, align 16, !tbaa !5
  br label %bb10

bb10:                                             ; preds = %bb61, %bb
  %tmp11 = phi i64 [ 2, %bb ], [ %tmp62, %bb61 ]
  %tmp12 = phi i64 [ 0, %bb ], [ %tmp28, %bb61 ]
  %tmp13 = phi i64 [ 0, %bb ], [ %tmp63, %bb61 ]
  %tmp14 = icmp ugt i64 %tmp11, %tmp12
  br i1 %tmp14, label %bb15, label %bb18

bb15:                                             ; preds = %bb10
  %tmp16 = add nuw nsw i64 %tmp12, 1
  %tmp17 = icmp eq i64 %tmp11, %tmp16
  br i1 %tmp17, label %bb65, label %bb24

bb18:                                             ; preds = %bb10
  %tmp19 = icmp eq i64 %tmp11, 0
  %tmp20 = icmp eq i64 %tmp12, 255
  %tmp21 = and i1 %tmp20, %tmp19
  br i1 %tmp21, label %bb65, label %bb22

bb22:                                             ; preds = %bb18
  %tmp23 = add nuw nsw i64 %tmp12, 1
  br label %bb24

bb24:                                             ; preds = %bb22, %bb15
  %tmp25 = phi i64 [ %tmp23, %bb22 ], [ %tmp16, %bb15 ]
  %tmp26 = getelementptr inbounds [256 x i64], [256 x i64]* %tmp, i64 0, i64 %tmp12
  %tmp27 = load i64, i64* %tmp26, align 8, !tbaa !5
  %tmp28 = and i64 %tmp25, 255
  %tmp29 = getelementptr inbounds %struct.node_t_struct, %struct.node_t_struct* %arg, i64 %tmp27, i32 0
  %tmp30 = load i64, i64* %tmp29, align 8, !tbaa !7
  %tmp31 = getelementptr inbounds %struct.node_t_struct, %struct.node_t_struct* %arg, i64 %tmp27, i32 1
  %tmp32 = load i64, i64* %tmp31, align 8, !tbaa !9
  %tmp33 = icmp ult i64 %tmp30, %tmp32
  br i1 %tmp33, label %bb34, label %bb61

bb34:                                             ; preds = %bb24
  %tmp35 = getelementptr inbounds i8, i8* %arg3, i64 %tmp27
  br label %bb36

bb36:                                             ; preds = %bb57, %bb34
  %tmp37 = phi i64 [ %tmp11, %bb34 ], [ %tmp58, %bb57 ]
  %tmp38 = phi i64 [ %tmp30, %bb34 ], [ %tmp59, %bb57 ]
  %tmp39 = getelementptr inbounds %struct.edge_t_struct, %struct.edge_t_struct* %arg1, i64 %tmp38, i32 0
  %tmp40 = load i64, i64* %tmp39, align 8, !tbaa !10
  %tmp41 = getelementptr inbounds i8, i8* %arg3, i64 %tmp40
  %tmp42 = load i8, i8* %tmp41, align 1, !tbaa !2
  %tmp43 = icmp eq i8 %tmp42, 127
  br i1 %tmp43, label %bb44, label %bb57

bb44:                                             ; preds = %bb36
  %tmp45 = load i8, i8* %tmp35, align 1, !tbaa !2
  %tmp46 = add i8 %tmp45, 1
  store i8 %tmp46, i8* %tmp41, align 1, !tbaa !2
  %tmp47 = sext i8 %tmp46 to i64
  %tmp48 = getelementptr inbounds i64, i64* %arg4, i64 %tmp47
  %tmp49 = load i64, i64* %tmp48, align 8, !tbaa !5
  %tmp50 = add i64 %tmp49, 1
  store i64 %tmp50, i64* %tmp48, align 8, !tbaa !5
  %tmp51 = icmp eq i64 %tmp37, 0
  %tmp52 = add i64 %tmp37, -1
  %tmp53 = select i1 %tmp51, i64 255, i64 %tmp52
  %tmp54 = getelementptr inbounds [256 x i64], [256 x i64]* %tmp, i64 0, i64 %tmp53
  store i64 %tmp40, i64* %tmp54, align 8, !tbaa !5
  %tmp55 = add i64 %tmp37, 1
  %tmp56 = and i64 %tmp55, 255
  br label %bb57

bb57:                                             ; preds = %bb44, %bb36
  %tmp58 = phi i64 [ %tmp56, %bb44 ], [ %tmp37, %bb36 ]
  %tmp59 = add nuw i64 %tmp38, 1
  %tmp60 = icmp eq i64 %tmp59, %tmp32
  br i1 %tmp60, label %bb61, label %bb36

bb61:                                             ; preds = %bb57, %bb24
  %tmp62 = phi i64 [ %tmp11, %bb24 ], [ %tmp58, %bb57 ]
  %tmp63 = add nuw nsw i64 %tmp13, 1
  %tmp64 = icmp ult i64 %tmp63, 256
  br i1 %tmp64, label %bb10, label %bb65

bb65:                                             ; preds = %bb61, %bb18, %bb15
  call void @llvm.memset.p0i8.i64(i8* %arg3, i8 127, i64 256, i32 1, i1 false)
  %tmp66 = getelementptr inbounds i64, i64* %arg4, i64 1
  %tmp67 = bitcast i64* %tmp66 to i8*
  call void @llvm.memset.p0i8.i64(i8* %tmp67, i8 0, i64 72, i32 8, i1 false)
  store i8 0, i8* %tmp6, align 1, !tbaa !2
  store i64 1, i64* %arg4, align 8, !tbaa !5
  store i64 %arg2, i64* %tmp7, align 16, !tbaa !5
  br label %bb68

bb68:                                             ; preds = %bb119, %bb65
  %tmp69 = phi i64 [ 2, %bb65 ], [ %tmp120, %bb119 ]
  %tmp70 = phi i64 [ 0, %bb65 ], [ %tmp86, %bb119 ]
  %tmp71 = phi i64 [ 0, %bb65 ], [ %tmp121, %bb119 ]
  %tmp72 = icmp ugt i64 %tmp69, %tmp70
  br i1 %tmp72, label %bb79, label %bb73

bb73:                                             ; preds = %bb68
  %tmp74 = icmp eq i64 %tmp69, 0
  %tmp75 = icmp eq i64 %tmp70, 255
  %tmp76 = and i1 %tmp75, %tmp74
  br i1 %tmp76, label %bb123, label %bb77

bb77:                                             ; preds = %bb73
  %tmp78 = add nuw nsw i64 %tmp70, 1
  br label %bb82

bb79:                                             ; preds = %bb68
  %tmp80 = add nuw nsw i64 %tmp70, 1
  %tmp81 = icmp eq i64 %tmp69, %tmp80
  br i1 %tmp81, label %bb123, label %bb82

bb82:                                             ; preds = %bb79, %bb77
  %tmp83 = phi i64 [ %tmp78, %bb77 ], [ %tmp80, %bb79 ]
  %tmp84 = getelementptr inbounds [256 x i64], [256 x i64]* %tmp, i64 0, i64 %tmp70
  %tmp85 = load i64, i64* %tmp84, align 8, !tbaa !5
  %tmp86 = and i64 %tmp83, 255
  %tmp87 = getelementptr inbounds %struct.node_t_struct, %struct.node_t_struct* %arg, i64 %tmp85, i32 0
  %tmp88 = load i64, i64* %tmp87, align 8, !tbaa !7
  %tmp89 = getelementptr inbounds %struct.node_t_struct, %struct.node_t_struct* %arg, i64 %tmp85, i32 1
  %tmp90 = load i64, i64* %tmp89, align 8, !tbaa !9
  %tmp91 = icmp ult i64 %tmp88, %tmp90
  br i1 %tmp91, label %bb92, label %bb119

bb92:                                             ; preds = %bb82
  %tmp93 = getelementptr inbounds i8, i8* %arg3, i64 %tmp85
  br label %bb94

bb94:                                             ; preds = %bb115, %bb92
  %tmp95 = phi i64 [ %tmp69, %bb92 ], [ %tmp116, %bb115 ]
  %tmp96 = phi i64 [ %tmp88, %bb92 ], [ %tmp117, %bb115 ]
  %tmp97 = getelementptr inbounds %struct.edge_t_struct, %struct.edge_t_struct* %arg1, i64 %tmp96, i32 0
  %tmp98 = load i64, i64* %tmp97, align 8, !tbaa !10
  %tmp99 = getelementptr inbounds i8, i8* %arg3, i64 %tmp98
  %tmp100 = load i8, i8* %tmp99, align 1, !tbaa !2
  %tmp101 = icmp eq i8 %tmp100, 127
  br i1 %tmp101, label %bb102, label %bb115

bb102:                                            ; preds = %bb94
  %tmp103 = load i8, i8* %tmp93, align 1, !tbaa !2
  %tmp104 = add i8 %tmp103, 1
  store i8 %tmp104, i8* %tmp99, align 1, !tbaa !2
  %tmp105 = sext i8 %tmp104 to i64
  %tmp106 = getelementptr inbounds i64, i64* %arg4, i64 %tmp105
  %tmp107 = load i64, i64* %tmp106, align 8, !tbaa !5
  %tmp108 = add i64 %tmp107, 1
  store i64 %tmp108, i64* %tmp106, align 8, !tbaa !5
  %tmp109 = icmp eq i64 %tmp95, 0
  %tmp110 = add i64 %tmp95, -1
  %tmp111 = select i1 %tmp109, i64 255, i64 %tmp110
  %tmp112 = getelementptr inbounds [256 x i64], [256 x i64]* %tmp, i64 0, i64 %tmp111
  store i64 %tmp98, i64* %tmp112, align 8, !tbaa !5
  %tmp113 = add i64 %tmp95, 1
  %tmp114 = and i64 %tmp113, 255
  br label %bb115

bb115:                                            ; preds = %bb102, %bb94
  %tmp116 = phi i64 [ %tmp114, %bb102 ], [ %tmp95, %bb94 ]
  %tmp117 = add nuw i64 %tmp96, 1
  %tmp118 = icmp eq i64 %tmp117, %tmp90
  br i1 %tmp118, label %bb119, label %bb94

bb119:                                            ; preds = %bb115, %bb82
  %tmp120 = phi i64 [ %tmp69, %bb82 ], [ %tmp116, %bb115 ]
  %tmp121 = add nuw nsw i64 %tmp71, 1
  %tmp122 = icmp ult i64 %tmp121, 256
  br i1 %tmp122, label %bb68, label %bb123

bb123:                                            ; preds = %bb119, %bb79, %bb73
  call void @llvm.lifetime.end.p0i8(i64 2048, i8* nonnull %tmp5) #7
  ret void
}

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.start.p0i8(i64, i8* nocapture) #1

; Function Attrs: argmemonly nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i32, i1) #1

; Function Attrs: argmemonly nounwind
declare void @llvm.lifetime.end.p0i8(i64, i8* nocapture) #1

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @bfs(%struct.node_t_struct* nocapture readonly %arg, %struct.edge_t_struct* nocapture readonly %arg1, i64 %arg2, i8* nocapture %arg3, i64* nocapture %arg4) #0 {
bb:
  tail call void @bfs_kernel(%struct.node_t_struct* %arg, %struct.edge_t_struct* %arg1, i64 %arg2, i8* %arg3, i64* %arg4)
  tail call void @bfs_kernel(%struct.node_t_struct* %arg, %struct.edge_t_struct* %arg1, i64 %arg2, i8* %arg3, i64* %arg4)
  ret void
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @run_benchmark(i8* %arg) #0 {
bb:
  %tmp = bitcast i8* %arg to %struct.node_t_struct*
  %tmp1 = getelementptr inbounds i8, i8* %arg, i64 4096
  %tmp2 = bitcast i8* %tmp1 to %struct.edge_t_struct*
  %tmp3 = getelementptr inbounds i8, i8* %arg, i64 36864
  %tmp4 = bitcast i8* %tmp3 to i64*
  %tmp5 = load i64, i64* %tmp4, align 8, !tbaa !12
  %tmp6 = getelementptr inbounds i8, i8* %arg, i64 36872
  %tmp7 = getelementptr inbounds i8, i8* %arg, i64 37128
  %tmp8 = bitcast i8* %tmp7 to i64*
  tail call void @bfs(%struct.node_t_struct* %tmp, %struct.edge_t_struct* nonnull %tmp2, i64 %tmp5, i8* nonnull %tmp6, i64* nonnull %tmp8) #7
  ret void
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @input_to_data(i32 %arg, i8* %arg1) #0 {
bb:
  tail call void @llvm.memset.p0i8.i64(i8* %arg1, i8 0, i64 37208, i32 1, i1 false)
  %tmp = getelementptr inbounds i8, i8* %arg1, i64 36872
  call void @llvm.memset.p0i8.i64(i8* nonnull %tmp, i8 127, i64 256, i32 1, i1 false)
  %tmp2 = tail call i8* @readfile(i32 %arg) #7
  %tmp3 = tail call i8* @find_section_start(i8* %tmp2, i32 1) #7
  %tmp4 = getelementptr inbounds i8, i8* %arg1, i64 36864
  %tmp5 = bitcast i8* %tmp4 to i64*
  %tmp6 = tail call i32 @parse_uint64_t_array(i8* %tmp3, i64* nonnull %tmp5, i32 1) #7
  %tmp7 = tail call i8* @find_section_start(i8* %tmp2, i32 2) #7
  %tmp8 = tail call noalias i8* @malloc(i64 4096) #7
  %tmp9 = bitcast i8* %tmp8 to i64*
  %tmp10 = tail call i32 @parse_uint64_t_array(i8* %tmp7, i64* %tmp9, i32 512) #7
  %tmp11 = bitcast i8* %arg1 to [256 x %struct.node_t_struct]*
  br label %bb12

bb12:                                             ; preds = %bb12, %bb
  %tmp13 = phi i64 [ 0, %bb ], [ %tmp41, %bb12 ]
  %tmp14 = shl nuw nsw i64 %tmp13, 1
  %tmp15 = getelementptr inbounds i64, i64* %tmp9, i64 %tmp14
  %tmp16 = getelementptr inbounds [256 x %struct.node_t_struct], [256 x %struct.node_t_struct]* %tmp11, i64 0, i64 %tmp13, i32 0
  %tmp17 = bitcast i64* %tmp15 to <2 x i64>*
  %tmp18 = load <2 x i64>, <2 x i64>* %tmp17, align 8, !tbaa !5
  %tmp19 = bitcast i64* %tmp16 to <2 x i64>*
  store <2 x i64> %tmp18, <2 x i64>* %tmp19, align 8, !tbaa !5
  %tmp20 = or i64 %tmp13, 1
  %tmp21 = shl nuw nsw i64 %tmp20, 1
  %tmp22 = getelementptr inbounds i64, i64* %tmp9, i64 %tmp21
  %tmp23 = getelementptr inbounds [256 x %struct.node_t_struct], [256 x %struct.node_t_struct]* %tmp11, i64 0, i64 %tmp20, i32 0
  %tmp24 = bitcast i64* %tmp22 to <2 x i64>*
  %tmp25 = load <2 x i64>, <2 x i64>* %tmp24, align 8, !tbaa !5
  %tmp26 = bitcast i64* %tmp23 to <2 x i64>*
  store <2 x i64> %tmp25, <2 x i64>* %tmp26, align 8, !tbaa !5
  %tmp27 = or i64 %tmp13, 2
  %tmp28 = shl nuw nsw i64 %tmp27, 1
  %tmp29 = getelementptr inbounds i64, i64* %tmp9, i64 %tmp28
  %tmp30 = getelementptr inbounds [256 x %struct.node_t_struct], [256 x %struct.node_t_struct]* %tmp11, i64 0, i64 %tmp27, i32 0
  %tmp31 = bitcast i64* %tmp29 to <2 x i64>*
  %tmp32 = load <2 x i64>, <2 x i64>* %tmp31, align 8, !tbaa !5
  %tmp33 = bitcast i64* %tmp30 to <2 x i64>*
  store <2 x i64> %tmp32, <2 x i64>* %tmp33, align 8, !tbaa !5
  %tmp34 = or i64 %tmp13, 3
  %tmp35 = shl nuw nsw i64 %tmp34, 1
  %tmp36 = getelementptr inbounds i64, i64* %tmp9, i64 %tmp35
  %tmp37 = getelementptr inbounds [256 x %struct.node_t_struct], [256 x %struct.node_t_struct]* %tmp11, i64 0, i64 %tmp34, i32 0
  %tmp38 = bitcast i64* %tmp36 to <2 x i64>*
  %tmp39 = load <2 x i64>, <2 x i64>* %tmp38, align 8, !tbaa !5
  %tmp40 = bitcast i64* %tmp37 to <2 x i64>*
  store <2 x i64> %tmp39, <2 x i64>* %tmp40, align 8, !tbaa !5
  %tmp41 = add nuw nsw i64 %tmp13, 4
  %tmp42 = icmp eq i64 %tmp41, 256
  br i1 %tmp42, label %bb43, label %bb12

bb43:                                             ; preds = %bb12
  tail call void @free(i8* nonnull %tmp8) #7
  %tmp44 = tail call i8* @find_section_start(i8* %tmp2, i32 3) #7
  %tmp45 = getelementptr inbounds i8, i8* %arg1, i64 4096
  %tmp46 = bitcast i8* %tmp45 to i64*
  %tmp47 = tail call i32 @parse_uint64_t_array(i8* %tmp44, i64* nonnull %tmp46, i32 4096) #7
  tail call void @free(i8* %tmp2) #7
  ret void
}

; Function Attrs: nounwind
declare noalias i8* @malloc(i64) local_unnamed_addr #2

; Function Attrs: nounwind
declare void @free(i8* nocapture) local_unnamed_addr #2

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @data_to_input(i32 %arg, i8* %arg1) #0 {
bb:
  %tmp = tail call i32 @write_section_header(i32 %arg) #7
  %tmp2 = getelementptr inbounds i8, i8* %arg1, i64 36864
  %tmp3 = bitcast i8* %tmp2 to i64*
  %tmp4 = tail call i32 @write_uint64_t_array(i32 %arg, i64* nonnull %tmp3, i32 1) #7
  %tmp5 = tail call i32 @write_section_header(i32 %arg) #7
  %tmp6 = tail call noalias i8* @malloc(i64 4096) #7
  %tmp7 = bitcast i8* %tmp6 to i64*
  %tmp8 = bitcast i8* %arg1 to [256 x %struct.node_t_struct]*
  br label %bb9

bb9:                                              ; preds = %bb9, %bb
  %tmp10 = phi i64 [ 0, %bb ], [ %tmp38, %bb9 ]
  %tmp11 = getelementptr inbounds [256 x %struct.node_t_struct], [256 x %struct.node_t_struct]* %tmp8, i64 0, i64 %tmp10, i32 0
  %tmp12 = shl nuw nsw i64 %tmp10, 1
  %tmp13 = getelementptr inbounds i64, i64* %tmp7, i64 %tmp12
  %tmp14 = bitcast i64* %tmp11 to <2 x i64>*
  %tmp15 = load <2 x i64>, <2 x i64>* %tmp14, align 8, !tbaa !5
  %tmp16 = bitcast i64* %tmp13 to <2 x i64>*
  store <2 x i64> %tmp15, <2 x i64>* %tmp16, align 8, !tbaa !5
  %tmp17 = or i64 %tmp10, 1
  %tmp18 = getelementptr inbounds [256 x %struct.node_t_struct], [256 x %struct.node_t_struct]* %tmp8, i64 0, i64 %tmp17, i32 0
  %tmp19 = shl nuw nsw i64 %tmp17, 1
  %tmp20 = getelementptr inbounds i64, i64* %tmp7, i64 %tmp19
  %tmp21 = bitcast i64* %tmp18 to <2 x i64>*
  %tmp22 = load <2 x i64>, <2 x i64>* %tmp21, align 8, !tbaa !5
  %tmp23 = bitcast i64* %tmp20 to <2 x i64>*
  store <2 x i64> %tmp22, <2 x i64>* %tmp23, align 8, !tbaa !5
  %tmp24 = or i64 %tmp10, 2
  %tmp25 = getelementptr inbounds [256 x %struct.node_t_struct], [256 x %struct.node_t_struct]* %tmp8, i64 0, i64 %tmp24, i32 0
  %tmp26 = shl nuw nsw i64 %tmp24, 1
  %tmp27 = getelementptr inbounds i64, i64* %tmp7, i64 %tmp26
  %tmp28 = bitcast i64* %tmp25 to <2 x i64>*
  %tmp29 = load <2 x i64>, <2 x i64>* %tmp28, align 8, !tbaa !5
  %tmp30 = bitcast i64* %tmp27 to <2 x i64>*
  store <2 x i64> %tmp29, <2 x i64>* %tmp30, align 8, !tbaa !5
  %tmp31 = or i64 %tmp10, 3
  %tmp32 = getelementptr inbounds [256 x %struct.node_t_struct], [256 x %struct.node_t_struct]* %tmp8, i64 0, i64 %tmp31, i32 0
  %tmp33 = shl nuw nsw i64 %tmp31, 1
  %tmp34 = getelementptr inbounds i64, i64* %tmp7, i64 %tmp33
  %tmp35 = bitcast i64* %tmp32 to <2 x i64>*
  %tmp36 = load <2 x i64>, <2 x i64>* %tmp35, align 8, !tbaa !5
  %tmp37 = bitcast i64* %tmp34 to <2 x i64>*
  store <2 x i64> %tmp36, <2 x i64>* %tmp37, align 8, !tbaa !5
  %tmp38 = add nuw nsw i64 %tmp10, 4
  %tmp39 = icmp eq i64 %tmp38, 256
  br i1 %tmp39, label %bb40, label %bb9

bb40:                                             ; preds = %bb9
  %tmp41 = tail call i32 @write_uint64_t_array(i32 %arg, i64* nonnull %tmp7, i32 512) #7
  tail call void @free(i8* nonnull %tmp6) #7
  %tmp42 = tail call i32 @write_section_header(i32 %arg) #7
  %tmp43 = getelementptr inbounds i8, i8* %arg1, i64 4096
  %tmp44 = bitcast i8* %tmp43 to i64*
  %tmp45 = tail call i32 @write_uint64_t_array(i32 %arg, i64* nonnull %tmp44, i32 4096) #7
  ret void
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @output_to_data(i32 %arg, i8* %arg1) #0 {
bb:
  tail call void @llvm.memset.p0i8.i64(i8* %arg1, i8 0, i64 37208, i32 1, i1 false)
  %tmp = tail call i8* @readfile(i32 %arg) #7
  %tmp2 = tail call i8* @find_section_start(i8* %tmp, i32 1) #7
  %tmp3 = getelementptr inbounds i8, i8* %arg1, i64 37128
  %tmp4 = bitcast i8* %tmp3 to i64*
  %tmp5 = tail call i32 @parse_uint64_t_array(i8* %tmp2, i64* nonnull %tmp4, i32 10) #7
  tail call void @free(i8* %tmp) #7
  ret void
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local void @data_to_output(i32 %arg, i8* %arg1) #0 {
bb:
  %tmp = tail call i32 @write_section_header(i32 %arg) #7
  %tmp2 = getelementptr inbounds i8, i8* %arg1, i64 37128
  %tmp3 = bitcast i8* %tmp2 to i64*
  %tmp4 = tail call i32 @write_uint64_t_array(i32 %arg, i64* nonnull %tmp3, i32 10) #7
  ret void
}

; Function Attrs: noinline norecurse nounwind readonly uwtable
define internal dso_local i32 @check_data(i8* nocapture readonly %arg, i8* nocapture readonly %arg1) #3 {
bb:
  %tmp = getelementptr inbounds i8, i8* %arg, i64 37128
  %tmp2 = getelementptr inbounds i8, i8* %arg1, i64 37128
  %tmp3 = bitcast i8* %tmp to i64*
  %tmp4 = load i64, i64* %tmp3, align 8, !tbaa !5
  %tmp5 = bitcast i8* %tmp2 to i64*
  %tmp6 = load i64, i64* %tmp5, align 8, !tbaa !5
  %tmp7 = icmp ne i64 %tmp4, %tmp6
  %tmp8 = getelementptr inbounds i8, i8* %arg, i64 37136
  %tmp9 = bitcast i8* %tmp8 to i64*
  %tmp10 = load i64, i64* %tmp9, align 8, !tbaa !5
  %tmp11 = getelementptr inbounds i8, i8* %arg1, i64 37136
  %tmp12 = bitcast i8* %tmp11 to i64*
  %tmp13 = load i64, i64* %tmp12, align 8, !tbaa !5
  %tmp14 = icmp ne i64 %tmp10, %tmp13
  %tmp15 = or i1 %tmp7, %tmp14
  %tmp16 = getelementptr inbounds i8, i8* %arg, i64 37144
  %tmp17 = bitcast i8* %tmp16 to i64*
  %tmp18 = load i64, i64* %tmp17, align 8, !tbaa !5
  %tmp19 = getelementptr inbounds i8, i8* %arg1, i64 37144
  %tmp20 = bitcast i8* %tmp19 to i64*
  %tmp21 = load i64, i64* %tmp20, align 8, !tbaa !5
  %tmp22 = icmp ne i64 %tmp18, %tmp21
  %tmp23 = or i1 %tmp15, %tmp22
  %tmp24 = getelementptr inbounds i8, i8* %arg, i64 37152
  %tmp25 = bitcast i8* %tmp24 to i64*
  %tmp26 = load i64, i64* %tmp25, align 8, !tbaa !5
  %tmp27 = getelementptr inbounds i8, i8* %arg1, i64 37152
  %tmp28 = bitcast i8* %tmp27 to i64*
  %tmp29 = load i64, i64* %tmp28, align 8, !tbaa !5
  %tmp30 = icmp ne i64 %tmp26, %tmp29
  %tmp31 = or i1 %tmp23, %tmp30
  %tmp32 = getelementptr inbounds i8, i8* %arg, i64 37160
  %tmp33 = bitcast i8* %tmp32 to i64*
  %tmp34 = load i64, i64* %tmp33, align 8, !tbaa !5
  %tmp35 = getelementptr inbounds i8, i8* %arg1, i64 37160
  %tmp36 = bitcast i8* %tmp35 to i64*
  %tmp37 = load i64, i64* %tmp36, align 8, !tbaa !5
  %tmp38 = icmp ne i64 %tmp34, %tmp37
  %tmp39 = or i1 %tmp31, %tmp38
  %tmp40 = getelementptr inbounds i8, i8* %arg, i64 37168
  %tmp41 = bitcast i8* %tmp40 to i64*
  %tmp42 = load i64, i64* %tmp41, align 8, !tbaa !5
  %tmp43 = getelementptr inbounds i8, i8* %arg1, i64 37168
  %tmp44 = bitcast i8* %tmp43 to i64*
  %tmp45 = load i64, i64* %tmp44, align 8, !tbaa !5
  %tmp46 = icmp ne i64 %tmp42, %tmp45
  %tmp47 = or i1 %tmp39, %tmp46
  %tmp48 = getelementptr inbounds i8, i8* %arg, i64 37176
  %tmp49 = bitcast i8* %tmp48 to i64*
  %tmp50 = load i64, i64* %tmp49, align 8, !tbaa !5
  %tmp51 = getelementptr inbounds i8, i8* %arg1, i64 37176
  %tmp52 = bitcast i8* %tmp51 to i64*
  %tmp53 = load i64, i64* %tmp52, align 8, !tbaa !5
  %tmp54 = icmp ne i64 %tmp50, %tmp53
  %tmp55 = or i1 %tmp47, %tmp54
  %tmp56 = getelementptr inbounds i8, i8* %arg, i64 37184
  %tmp57 = bitcast i8* %tmp56 to i64*
  %tmp58 = load i64, i64* %tmp57, align 8, !tbaa !5
  %tmp59 = getelementptr inbounds i8, i8* %arg1, i64 37184
  %tmp60 = bitcast i8* %tmp59 to i64*
  %tmp61 = load i64, i64* %tmp60, align 8, !tbaa !5
  %tmp62 = icmp ne i64 %tmp58, %tmp61
  %tmp63 = or i1 %tmp55, %tmp62
  %tmp64 = getelementptr inbounds i8, i8* %arg, i64 37192
  %tmp65 = bitcast i8* %tmp64 to i64*
  %tmp66 = load i64, i64* %tmp65, align 8, !tbaa !5
  %tmp67 = getelementptr inbounds i8, i8* %arg1, i64 37192
  %tmp68 = bitcast i8* %tmp67 to i64*
  %tmp69 = load i64, i64* %tmp68, align 8, !tbaa !5
  %tmp70 = icmp ne i64 %tmp66, %tmp69
  %tmp71 = or i1 %tmp63, %tmp70
  %tmp72 = getelementptr inbounds i8, i8* %arg, i64 37200
  %tmp73 = bitcast i8* %tmp72 to i64*
  %tmp74 = load i64, i64* %tmp73, align 8, !tbaa !5
  %tmp75 = getelementptr inbounds i8, i8* %arg1, i64 37200
  %tmp76 = bitcast i8* %tmp75 to i64*
  %tmp77 = load i64, i64* %tmp76, align 8, !tbaa !5
  %tmp78 = icmp ne i64 %tmp74, %tmp77
  %tmp79 = or i1 %tmp71, %tmp78
  %tmp80 = xor i1 %tmp79, true
  %tmp81 = zext i1 %tmp80 to i32
  ret i32 %tmp81
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local noalias i8* @readfile(i32 %arg) #0 {
bb:
  %tmp = alloca %struct.stat, align 8
  %tmp1 = bitcast %struct.stat* %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 144, i8* nonnull %tmp1) #7
  %tmp2 = icmp sgt i32 %arg, 1
  br i1 %tmp2, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 40, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #8
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = call i32 @fstat(i32 %arg, %struct.stat* %tmp) #7
  %tmp6 = icmp eq i32 %tmp5, 0
  br i1 %tmp6, label %bb8, label %bb7

bb7:                                              ; preds = %bb4
  call void @__assert_fail(i8* getelementptr inbounds ([51 x i8], [51 x i8]* @.str.4, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 41, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #8
  unreachable

bb8:                                              ; preds = %bb4
  %tmp9 = getelementptr inbounds %struct.stat, %struct.stat* %tmp, i64 0, i32 8
  %tmp10 = load i64, i64* %tmp9, align 8, !tbaa !14
  %tmp11 = icmp sgt i64 %tmp10, 0
  br i1 %tmp11, label %bb13, label %bb12

bb12:                                             ; preds = %bb8
  call void @__assert_fail(i8* getelementptr inbounds ([25 x i8], [25 x i8]* @.str.6, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 43, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #8
  unreachable

bb13:                                             ; preds = %bb8
  %tmp14 = add nsw i64 %tmp10, 1
  %tmp15 = call noalias i8* @malloc(i64 %tmp14) #7
  br label %bb18

bb16:                                             ; preds = %bb18
  %tmp17 = icmp sgt i64 %tmp10, %tmp24
  br i1 %tmp17, label %bb18, label %bb26

bb18:                                             ; preds = %bb16, %bb13
  %tmp19 = phi i64 [ 0, %bb13 ], [ %tmp24, %bb16 ]
  %tmp20 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp19
  %tmp21 = sub nsw i64 %tmp10, %tmp19
  %tmp22 = call i64 @read(i32 %arg, i8* %tmp20, i64 %tmp21) #7
  %tmp23 = icmp sgt i64 %tmp22, -1
  %tmp24 = add nsw i64 %tmp22, %tmp19
  br i1 %tmp23, label %bb16, label %bb25

bb25:                                             ; preds = %bb18
  call void @__assert_fail(i8* getelementptr inbounds ([29 x i8], [29 x i8]* @.str.8, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 48, i8* getelementptr inbounds ([20 x i8], [20 x i8]* @__PRETTY_FUNCTION__.readfile, i64 0, i64 0)) #8
  unreachable

bb26:                                             ; preds = %bb16
  %tmp27 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp10
  store i8 0, i8* %tmp27, align 1, !tbaa !2
  %tmp28 = call i32 @close(i32 %arg) #7
  call void @llvm.lifetime.end.p0i8(i64 144, i8* nonnull %tmp1) #7
  ret i8* %tmp15
}

; Function Attrs: noreturn nounwind
declare void @__assert_fail(i8*, i8*, i32, i8*) local_unnamed_addr #4

; Function Attrs: noinline nounwind uwtable
define available_externally dso_local i32 @fstat(i32 %arg, %struct.stat* nonnull %arg1) local_unnamed_addr #0 {
bb:
  %tmp = tail call i32 @__fxstat(i32 1, i32 %arg, %struct.stat* nonnull %arg1) #7
  ret i32 %tmp
}

declare i64 @read(i32, i8* nocapture, i64) local_unnamed_addr #5

declare i32 @close(i32) local_unnamed_addr #5

; Function Attrs: nounwind
declare i32 @__fxstat(i32, i32, %struct.stat*) local_unnamed_addr #2

; Function Attrs: noinline nounwind uwtable
define internal dso_local i8* @find_section_start(i8* readonly %arg, i32 %arg1) #0 {
bb:
  %tmp = icmp sgt i32 %arg1, -1
  br i1 %tmp, label %bb3, label %bb2

bb2:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([33 x i8], [33 x i8]* @.str.10, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 59, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.find_section_start, i64 0, i64 0)) #8
  unreachable

bb3:                                              ; preds = %bb
  %tmp4 = icmp eq i32 %arg1, 0
  br i1 %tmp4, label %bb34, label %bb5

bb5:                                              ; preds = %bb3
  %tmp6 = load i8, i8* %arg, align 1, !tbaa !2
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
  %tmp13 = load i8, i8* %tmp12, align 1, !tbaa !2
  br label %bb24

bb14:                                             ; preds = %bb7
  %tmp15 = getelementptr inbounds i8, i8* %tmp10, i64 1
  %tmp16 = load i8, i8* %tmp15, align 1, !tbaa !2
  %tmp17 = icmp eq i8 %tmp16, 37
  br i1 %tmp17, label %bb18, label %bb24

bb18:                                             ; preds = %bb14
  %tmp19 = getelementptr inbounds i8, i8* %tmp10, i64 2
  %tmp20 = load i8, i8* %tmp19, align 1, !tbaa !2
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
define internal dso_local i32 @parse_string(i8* readonly %arg, i8* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp eq i8* %arg, null
  br i1 %tmp, label %bb3, label %bb4

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 79, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.parse_string, i64 0, i64 0)) #8
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp slt i32 %arg2, 0
  br i1 %tmp5, label %bb8, label %bb6

bb6:                                              ; preds = %bb4
  %tmp7 = sext i32 %arg2 to i64
  tail call void @llvm.memcpy.p0i8.p0i8.i64(i8* %arg1, i8* nonnull %arg, i64 %tmp7, i32 1, i1 false)
  br label %bb47

bb8:                                              ; preds = %bb4
  %tmp9 = load i8, i8* %arg, align 1, !tbaa !2
  %tmp10 = icmp eq i8 %tmp9, 0
  br i1 %tmp10, label %bb43, label %bb11

bb11:                                             ; preds = %bb8
  %tmp12 = getelementptr inbounds i8, i8* %arg, i64 1
  %tmp13 = load i8, i8* %tmp12, align 1, !tbaa !2
  br label %bb17

bb14:                                             ; preds = %bb31
  %tmp15 = load i8, i8* %tmp24, align 1, !tbaa !2
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
  %tmp29 = load i8, i8* %tmp28, align 1, !tbaa !2
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
  store i8 0, i8* %tmp46, align 1, !tbaa !2
  br label %bb47

bb47:                                             ; preds = %bb43, %bb6
  ret i32 0
}

; Function Attrs: argmemonly nounwind
declare void @llvm.memcpy.p0i8.p0i8.i64(i8* nocapture writeonly, i8* nocapture readonly, i64, i32, i1) #1

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_uint8_t_array(i8* %arg, i8* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #7
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 132, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_uint8_t_array, i64 0, i64 0)) #8
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !18
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #7
  %tmp17 = trunc i64 %tmp16 to i8
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !18
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !2
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !18
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #9
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i8, i8* %arg1, i64 %tmp14
  store i8 %tmp17, i8* %tmp26, align 1, !tbaa !2
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #10
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !2
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #10
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !2
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #7
  ret i32 0
}

; Function Attrs: nounwind
declare i8* @strtok(i8*, i8* nocapture readonly) local_unnamed_addr #2

; Function Attrs: nounwind
declare i64 @strtol(i8* readonly, i8** nocapture, i32) local_unnamed_addr #2

; Function Attrs: nounwind
declare i32 @fprintf(%struct._IO_FILE* nocapture, i8* nocapture readonly, ...) local_unnamed_addr #2

; Function Attrs: argmemonly nounwind readonly
declare i64 @strlen(i8* nocapture) local_unnamed_addr #6

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_uint16_t_array(i8* %arg, i16* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #7
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 133, i8* getelementptr inbounds ([50 x i8], [50 x i8]* @__PRETTY_FUNCTION__.parse_uint16_t_array, i64 0, i64 0)) #8
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !18
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #7
  %tmp17 = trunc i64 %tmp16 to i16
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !18
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !2
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !18
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #9
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i16, i16* %arg1, i64 %tmp14
  store i16 %tmp17, i16* %tmp26, align 2, !tbaa !20
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #10
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !2
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #10
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !2
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #7
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_uint32_t_array(i8* %arg, i32* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #7
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 134, i8* getelementptr inbounds ([50 x i8], [50 x i8]* @__PRETTY_FUNCTION__.parse_uint32_t_array, i64 0, i64 0)) #8
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !18
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #7
  %tmp17 = trunc i64 %tmp16 to i32
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !18
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !2
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !18
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #9
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i32, i32* %arg1, i64 %tmp14
  store i32 %tmp17, i32* %tmp26, align 4, !tbaa !22
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #10
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !2
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #10
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !2
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #7
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_uint64_t_array(i8* %arg, i64* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #7
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 135, i8* getelementptr inbounds ([50 x i8], [50 x i8]* @__PRETTY_FUNCTION__.parse_uint64_t_array, i64 0, i64 0)) #8
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !18
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #7
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !18
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !2
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !18
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #9
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds i64, i64* %arg1, i64 %tmp14
  store i64 %tmp16, i64* %tmp25, align 8, !tbaa !5
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #10
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !2
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #10
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !2
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #7
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_int8_t_array(i8* %arg, i8* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #7
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 136, i8* getelementptr inbounds ([46 x i8], [46 x i8]* @__PRETTY_FUNCTION__.parse_int8_t_array, i64 0, i64 0)) #8
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !18
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #7
  %tmp17 = trunc i64 %tmp16 to i8
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !18
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !2
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !18
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #9
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i8, i8* %arg1, i64 %tmp14
  store i8 %tmp17, i8* %tmp26, align 1, !tbaa !2
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #10
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !2
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #10
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !2
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #7
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_int16_t_array(i8* %arg, i16* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #7
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 137, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_int16_t_array, i64 0, i64 0)) #8
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !18
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #7
  %tmp17 = trunc i64 %tmp16 to i16
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !18
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !2
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !18
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #9
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i16, i16* %arg1, i64 %tmp14
  store i16 %tmp17, i16* %tmp26, align 2, !tbaa !20
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #10
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !2
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #10
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !2
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #7
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_int32_t_array(i8* %arg, i32* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #7
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 138, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_int32_t_array, i64 0, i64 0)) #8
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !18
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #7
  %tmp17 = trunc i64 %tmp16 to i32
  %tmp18 = load i8*, i8** %tmp, align 8, !tbaa !18
  %tmp19 = load i8, i8* %tmp18, align 1, !tbaa !2
  %tmp20 = icmp eq i8 %tmp19, 0
  br i1 %tmp20, label %bb25, label %bb21

bb21:                                             ; preds = %bb13
  %tmp22 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !18
  %tmp23 = trunc i64 %tmp14 to i32
  %tmp24 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp22, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp23) #9
  br label %bb25

bb25:                                             ; preds = %bb21, %bb13
  %tmp26 = getelementptr inbounds i32, i32* %arg1, i64 %tmp14
  store i32 %tmp17, i32* %tmp26, align 4, !tbaa !22
  %tmp27 = add nuw nsw i64 %tmp14, 1
  %tmp28 = tail call i64 @strlen(i8* nonnull %tmp15) #10
  %tmp29 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp28
  store i8 10, i8* %tmp29, align 1, !tbaa !2
  %tmp30 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
  %tmp31 = icmp ne i8* %tmp30, null
  %tmp32 = icmp slt i64 %tmp27, %tmp12
  %tmp33 = and i1 %tmp32, %tmp31
  br i1 %tmp33, label %bb13, label %bb34

bb34:                                             ; preds = %bb25, %bb6
  %tmp35 = phi i8* [ %tmp7, %bb6 ], [ %tmp30, %bb25 ]
  %tmp36 = phi i1 [ %tmp8, %bb6 ], [ %tmp31, %bb25 ]
  br i1 %tmp36, label %bb37, label %bb40

bb37:                                             ; preds = %bb34
  %tmp38 = tail call i64 @strlen(i8* nonnull %tmp35) #10
  %tmp39 = getelementptr inbounds i8, i8* %tmp35, i64 %tmp38
  store i8 10, i8* %tmp39, align 1, !tbaa !2
  br label %bb40

bb40:                                             ; preds = %bb37, %bb34
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #7
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_int64_t_array(i8* %arg, i64* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #7
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 139, i8* getelementptr inbounds ([48 x i8], [48 x i8]* @__PRETTY_FUNCTION__.parse_int64_t_array, i64 0, i64 0)) #8
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !18
  %tmp16 = call i64 @strtol(i8* nonnull %tmp15, i8** nonnull %tmp, i32 10) #7
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !18
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !2
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !18
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #9
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds i64, i64* %arg1, i64 %tmp14
  store i64 %tmp16, i64* %tmp25, align 8, !tbaa !5
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #10
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !2
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #10
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !2
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #7
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_float_array(i8* %arg, float* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #7
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 141, i8* getelementptr inbounds ([44 x i8], [44 x i8]* @__PRETTY_FUNCTION__.parse_float_array, i64 0, i64 0)) #8
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !18
  %tmp16 = call float @strtof(i8* nonnull %tmp15, i8** nonnull %tmp) #7
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !18
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !2
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !18
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #9
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds float, float* %arg1, i64 %tmp14
  store float %tmp16, float* %tmp25, align 4, !tbaa !23
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #10
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !2
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #10
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !2
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #7
  ret i32 0
}

; Function Attrs: nounwind
declare float @strtof(i8* readonly, i8** nocapture) local_unnamed_addr #2

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @parse_double_array(i8* %arg, double* nocapture %arg1, i32 %arg2) #0 {
bb:
  %tmp = alloca i8*, align 8
  %tmp3 = bitcast i8** %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 8, i8* nonnull %tmp3) #7
  %tmp4 = icmp eq i8* %arg, null
  br i1 %tmp4, label %bb5, label %bb6

bb5:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.12, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 142, i8* getelementptr inbounds ([46 x i8], [46 x i8]* @__PRETTY_FUNCTION__.parse_double_array, i64 0, i64 0)) #8
  unreachable

bb6:                                              ; preds = %bb
  %tmp7 = tail call i8* @strtok(i8* nonnull %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
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
  store i8* %tmp15, i8** %tmp, align 8, !tbaa !18
  %tmp16 = call double @strtod(i8* nonnull %tmp15, i8** nonnull %tmp) #7
  %tmp17 = load i8*, i8** %tmp, align 8, !tbaa !18
  %tmp18 = load i8, i8* %tmp17, align 1, !tbaa !2
  %tmp19 = icmp eq i8 %tmp18, 0
  br i1 %tmp19, label %bb24, label %bb20

bb20:                                             ; preds = %bb13
  %tmp21 = load %struct._IO_FILE*, %struct._IO_FILE** @stderr, align 8, !tbaa !18
  %tmp22 = trunc i64 %tmp14 to i32
  %tmp23 = tail call i32 (%struct._IO_FILE*, i8*, ...) @fprintf(%struct._IO_FILE* %tmp21, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @.str.14, i64 0, i64 0), i32 %tmp22) #9
  br label %bb24

bb24:                                             ; preds = %bb20, %bb13
  %tmp25 = getelementptr inbounds double, double* %arg1, i64 %tmp14
  store double %tmp16, double* %tmp25, align 8, !tbaa !25
  %tmp26 = add nuw nsw i64 %tmp14, 1
  %tmp27 = tail call i64 @strlen(i8* nonnull %tmp15) #10
  %tmp28 = getelementptr inbounds i8, i8* %tmp15, i64 %tmp27
  store i8 10, i8* %tmp28, align 1, !tbaa !2
  %tmp29 = tail call i8* @strtok(i8* null, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0)) #7
  %tmp30 = icmp ne i8* %tmp29, null
  %tmp31 = icmp slt i64 %tmp26, %tmp12
  %tmp32 = and i1 %tmp31, %tmp30
  br i1 %tmp32, label %bb13, label %bb33

bb33:                                             ; preds = %bb24, %bb6
  %tmp34 = phi i8* [ %tmp7, %bb6 ], [ %tmp29, %bb24 ]
  %tmp35 = phi i1 [ %tmp8, %bb6 ], [ %tmp30, %bb24 ]
  br i1 %tmp35, label %bb36, label %bb39

bb36:                                             ; preds = %bb33
  %tmp37 = tail call i64 @strlen(i8* nonnull %tmp34) #10
  %tmp38 = getelementptr inbounds i8, i8* %tmp34, i64 %tmp37
  store i8 10, i8* %tmp38, align 1, !tbaa !2
  br label %bb39

bb39:                                             ; preds = %bb36, %bb33
  call void @llvm.lifetime.end.p0i8(i64 8, i8* nonnull %tmp3) #7
  ret i32 0
}

; Function Attrs: nounwind
declare double @strtod(i8* readonly, i8** nocapture) local_unnamed_addr #2

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_string(i32 %arg, i8* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 147, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @__PRETTY_FUNCTION__.write_string, i64 0, i64 0)) #8
  unreachable

bb4:                                              ; preds = %bb
  %tmp5 = icmp slt i32 %arg2, 0
  br i1 %tmp5, label %bb6, label %bb9

bb6:                                              ; preds = %bb4
  %tmp7 = tail call i64 @strlen(i8* %arg1) #10
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
  %tmp22 = tail call i64 @write(i32 %arg, i8* %tmp19, i64 %tmp21) #7
  %tmp23 = trunc i64 %tmp22 to i32
  %tmp24 = icmp sgt i32 %tmp23, -1
  %tmp25 = add nsw i32 %tmp17, %tmp23
  br i1 %tmp24, label %bb14, label %bb26

bb26:                                             ; preds = %bb16
  tail call void @__assert_fail(i8* getelementptr inbounds ([28 x i8], [28 x i8]* @.str.16, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 154, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @__PRETTY_FUNCTION__.write_string, i64 0, i64 0)) #8
  unreachable

bb27:                                             ; preds = %bb32, %bb13
  %tmp28 = tail call i64 @write(i32 %arg, i8* getelementptr inbounds ([2 x i8], [2 x i8]* @.str.13, i64 0, i64 0), i64 1) #7
  %tmp29 = trunc i64 %tmp28 to i32
  %tmp30 = icmp sgt i32 %tmp29, -1
  br i1 %tmp30, label %bb32, label %bb31

bb31:                                             ; preds = %bb27
  tail call void @__assert_fail(i8* getelementptr inbounds ([28 x i8], [28 x i8]* @.str.16, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 160, i8* getelementptr inbounds ([35 x i8], [35 x i8]* @__PRETTY_FUNCTION__.write_string, i64 0, i64 0)) #8
  unreachable

bb32:                                             ; preds = %bb27
  %tmp33 = icmp eq i32 %tmp29, 0
  br i1 %tmp33, label %bb27, label %bb34

bb34:                                             ; preds = %bb32
  ret i32 0
}

declare i64 @write(i32, i8* nocapture readonly, i64) local_unnamed_addr #5

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_uint8_t_array(i32 %arg, i8* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 177, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_uint8_t_array, i64 0, i64 0)) #8
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
  %tmp11 = load i8, i8* %tmp10, align 1, !tbaa !2
  %tmp12 = zext i8 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.17, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal void @fd_printf(i32 %arg, i8* nocapture readonly %arg1, ...) unnamed_addr #0 {
bb:
  %tmp = alloca [1 x %struct.__va_list_tag], align 16
  %tmp2 = alloca [256 x i8], align 16
  %tmp3 = bitcast [1 x %struct.__va_list_tag]* %tmp to i8*
  call void @llvm.lifetime.start.p0i8(i64 24, i8* nonnull %tmp3) #7
  %tmp4 = getelementptr inbounds [256 x i8], [256 x i8]* %tmp2, i64 0, i64 0
  call void @llvm.lifetime.start.p0i8(i64 256, i8* nonnull %tmp4) #7
  %tmp5 = getelementptr inbounds [1 x %struct.__va_list_tag], [1 x %struct.__va_list_tag]* %tmp, i64 0, i64 0
  call void @llvm.va_start(i8* nonnull %tmp3)
  %tmp6 = call i32 @vsnprintf(i8* nonnull %tmp4, i64 256, i8* %arg1, %struct.__va_list_tag* nonnull %tmp5) #7
  call void @llvm.va_end(i8* nonnull %tmp3)
  %tmp7 = icmp slt i32 %tmp6, 256
  br i1 %tmp7, label %bb9, label %bb8

bb8:                                              ; preds = %bb
  call void @__assert_fail(i8* getelementptr inbounds ([90 x i8], [90 x i8]* @.str.24, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 22, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.fd_printf, i64 0, i64 0)) #8
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
  %tmp20 = call i64 @write(i32 %arg, i8* nonnull %tmp17, i64 %tmp19) #7
  %tmp21 = trunc i64 %tmp20 to i32
  %tmp22 = icmp sgt i32 %tmp21, -1
  %tmp23 = add nsw i32 %tmp15, %tmp21
  br i1 %tmp22, label %bb12, label %bb24

bb24:                                             ; preds = %bb14
  call void @__assert_fail(i8* getelementptr inbounds ([28 x i8], [28 x i8]* @.str.16, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 26, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.fd_printf, i64 0, i64 0)) #8
  unreachable

bb25:                                             ; preds = %bb12, %bb9
  %tmp26 = phi i32 [ 0, %bb9 ], [ %tmp23, %bb12 ]
  %tmp27 = icmp eq i32 %tmp6, %tmp26
  br i1 %tmp27, label %bb29, label %bb28

bb28:                                             ; preds = %bb25
  call void @__assert_fail(i8* getelementptr inbounds ([50 x i8], [50 x i8]* @.str.26, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 29, i8* getelementptr inbounds ([38 x i8], [38 x i8]* @__PRETTY_FUNCTION__.fd_printf, i64 0, i64 0)) #8
  unreachable

bb29:                                             ; preds = %bb25
  call void @llvm.lifetime.end.p0i8(i64 256, i8* nonnull %tmp4) #7
  call void @llvm.lifetime.end.p0i8(i64 24, i8* nonnull %tmp3) #7
  ret void
}

; Function Attrs: nounwind
declare void @llvm.va_start(i8*) #7

; Function Attrs: nounwind
declare i32 @vsnprintf(i8* nocapture, i64, i8* nocapture readonly, %struct.__va_list_tag*) local_unnamed_addr #2

; Function Attrs: nounwind
declare void @llvm.va_end(i8*) #7

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_uint16_t_array(i32 %arg, i16* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 178, i8* getelementptr inbounds ([47 x i8], [47 x i8]* @__PRETTY_FUNCTION__.write_uint16_t_array, i64 0, i64 0)) #8
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
  %tmp11 = load i16, i16* %tmp10, align 2, !tbaa !20
  %tmp12 = zext i16 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.17, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_uint32_t_array(i32 %arg, i32* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 179, i8* getelementptr inbounds ([47 x i8], [47 x i8]* @__PRETTY_FUNCTION__.write_uint32_t_array, i64 0, i64 0)) #8
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
  %tmp11 = load i32, i32* %tmp10, align 4, !tbaa !22
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.17, i64 0, i64 0), i32 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_uint64_t_array(i32 %arg, i64* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 180, i8* getelementptr inbounds ([47 x i8], [47 x i8]* @__PRETTY_FUNCTION__.write_uint64_t_array, i64 0, i64 0)) #8
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
  %tmp11 = load i64, i64* %tmp10, align 8, !tbaa !5
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str.18, i64 0, i64 0), i64 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_int8_t_array(i32 %arg, i8* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 181, i8* getelementptr inbounds ([43 x i8], [43 x i8]* @__PRETTY_FUNCTION__.write_int8_t_array, i64 0, i64 0)) #8
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
  %tmp11 = load i8, i8* %tmp10, align 1, !tbaa !2
  %tmp12 = sext i8 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.19, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_int16_t_array(i32 %arg, i16* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 182, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_int16_t_array, i64 0, i64 0)) #8
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
  %tmp11 = load i16, i16* %tmp10, align 2, !tbaa !20
  %tmp12 = sext i16 %tmp11 to i32
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.19, i64 0, i64 0), i32 %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_int32_t_array(i32 %arg, i32* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 183, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_int32_t_array, i64 0, i64 0)) #8
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
  %tmp11 = load i32, i32* %tmp10, align 4, !tbaa !22
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([4 x i8], [4 x i8]* @.str.19, i64 0, i64 0), i32 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_int64_t_array(i32 %arg, i64* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 184, i8* getelementptr inbounds ([45 x i8], [45 x i8]* @__PRETTY_FUNCTION__.write_int64_t_array, i64 0, i64 0)) #8
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
  %tmp11 = load i64, i64* %tmp10, align 8, !tbaa !5
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([5 x i8], [5 x i8]* @.str.20, i64 0, i64 0), i64 %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_float_array(i32 %arg, float* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 186, i8* getelementptr inbounds ([41 x i8], [41 x i8]* @__PRETTY_FUNCTION__.write_float_array, i64 0, i64 0)) #8
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
  %tmp11 = load float, float* %tmp10, align 4, !tbaa !23
  %tmp12 = fpext float %tmp11 to double
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.21, i64 0, i64 0), double %tmp12)
  %tmp13 = add nuw nsw i64 %tmp9, 1
  %tmp14 = icmp eq i64 %tmp13, %tmp7
  br i1 %tmp14, label %bb15, label %bb8

bb15:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_double_array(i32 %arg, double* nocapture readonly %arg1, i32 %arg2) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb4, label %bb3

bb3:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 187, i8* getelementptr inbounds ([43 x i8], [43 x i8]* @__PRETTY_FUNCTION__.write_double_array, i64 0, i64 0)) #8
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
  %tmp11 = load double, double* %tmp10, align 8, !tbaa !25
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([7 x i8], [7 x i8]* @.str.21, i64 0, i64 0), double %tmp11)
  %tmp12 = add nuw nsw i64 %tmp9, 1
  %tmp13 = icmp eq i64 %tmp12, %tmp7
  br i1 %tmp13, label %bb14, label %bb8

bb14:                                             ; preds = %bb8, %bb4
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define internal dso_local i32 @write_section_header(i32 %arg) #0 {
bb:
  %tmp = icmp sgt i32 %arg, 1
  br i1 %tmp, label %bb2, label %bb1

bb1:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([34 x i8], [34 x i8]* @.str.1, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2, i64 0, i64 0), i32 190, i8* getelementptr inbounds ([30 x i8], [30 x i8]* @__PRETTY_FUNCTION__.write_section_header, i64 0, i64 0)) #8
  unreachable

bb2:                                              ; preds = %bb
  tail call void (i32, i8*, ...) @fd_printf(i32 %arg, i8* getelementptr inbounds ([6 x i8], [6 x i8]* @.str.22, i64 0, i64 0))
  ret i32 0
}

; Function Attrs: noinline nounwind uwtable
define dso_local i32 @main(i32 %arg, i8** nocapture readonly %arg1) #0 {
bb:
  %tmp = icmp slt i32 %arg, 4
  br i1 %tmp, label %bb3, label %bb2

bb2:                                              ; preds = %bb
  tail call void @__assert_fail(i8* getelementptr inbounds ([57 x i8], [57 x i8]* @.str.1.11, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.12, i64 0, i64 0), i32 21, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #8
  unreachable

bb3:                                              ; preds = %bb
  %tmp4 = icmp sgt i32 %arg, 1
  br i1 %tmp4, label %bb5, label %bb8

bb5:                                              ; preds = %bb3
  %tmp6 = getelementptr inbounds i8*, i8** %arg1, i64 1
  %tmp7 = load i8*, i8** %tmp6, align 8, !tbaa !18
  br label %bb8

bb8:                                              ; preds = %bb5, %bb3
  %tmp9 = phi i8* [ %tmp7, %bb5 ], [ getelementptr inbounds ([11 x i8], [11 x i8]* @.str.3, i64 0, i64 0), %bb3 ]
  %tmp10 = load i32, i32* @INPUT_SIZE, align 4, !tbaa !22
  %tmp11 = sext i32 %tmp10 to i64
  %tmp12 = tail call noalias i8* @malloc(i64 %tmp11) #7
  %tmp13 = icmp eq i8* %tmp12, null
  br i1 %tmp13, label %bb14, label %bb15

bb14:                                             ; preds = %bb8
  tail call void @__assert_fail(i8* getelementptr inbounds ([30 x i8], [30 x i8]* @.str.5, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.12, i64 0, i64 0), i32 37, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #8
  unreachable

bb15:                                             ; preds = %bb8
  %tmp16 = tail call i32 (i8*, i32, ...) @open(i8* %tmp9, i32 0) #7
  %tmp17 = icmp sgt i32 %tmp16, 0
  br i1 %tmp17, label %bb19, label %bb18

bb18:                                             ; preds = %bb15
  tail call void @__assert_fail(i8* getelementptr inbounds ([43 x i8], [43 x i8]* @.str.7, i64 0, i64 0), i8* getelementptr inbounds ([23 x i8], [23 x i8]* @.str.2.12, i64 0, i64 0), i32 39, i8* getelementptr inbounds ([23 x i8], [23 x i8]* @__PRETTY_FUNCTION__.main, i64 0, i64 0)) #8
  unreachable

bb19:                                             ; preds = %bb15
  tail call void @input_to_data(i32 %tmp16, i8* nonnull %tmp12) #7
  tail call void @run_benchmark(i8* nonnull %tmp12) #7
  tail call void @free(i8* nonnull %tmp12) #7
  %tmp20 = tail call i32 @puts(i8* getelementptr inbounds ([9 x i8], [9 x i8]* @str, i64 0, i64 0))
  ret i32 0
}

declare i32 @open(i8* nocapture readonly, i32, ...) local_unnamed_addr #5

; Function Attrs: nounwind
declare i32 @puts(i8* nocapture readonly) local_unnamed_addr #7

attributes #0 = { noinline nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }
attributes #2 = { nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #3 = { noinline norecurse nounwind readonly uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #4 = { noreturn nounwind "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #5 = { "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #6 = { argmemonly nounwind readonly "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #7 = { nounwind }
attributes #8 = { noreturn nounwind }
attributes #9 = { cold }
attributes #10 = { nounwind readonly }

!llvm.ident = !{!0, !0, !0, !0}
!llvm.module.flags = !{!1}

!0 = !{!"clang version 6.0.0 (git@github.com:seanzw/clang.git bb8d45f8ab88237f1fa0530b8ad9b96bf4a5e6cc) (git@github.com:seanzw/llvm.git 16ebb58ea40d384e8daa4c48d2bf7dd1ccfa5fcd)"}
!1 = !{i32 1, !"wchar_size", i32 4}
!2 = !{!3, !3, i64 0}
!3 = !{!"omnipotent char", !4, i64 0}
!4 = !{!"Simple C/C++ TBAA"}
!5 = !{!6, !6, i64 0}
!6 = !{!"long", !3, i64 0}
!7 = !{!8, !6, i64 0}
!8 = !{!"node_t_struct", !6, i64 0, !6, i64 8}
!9 = !{!8, !6, i64 8}
!10 = !{!11, !6, i64 0}
!11 = !{!"edge_t_struct", !6, i64 0}
!12 = !{!13, !6, i64 36864}
!13 = !{!"bench_args_t", !3, i64 0, !3, i64 4096, !6, i64 36864, !3, i64 36872, !3, i64 37128}
!14 = !{!15, !6, i64 48}
!15 = !{!"stat", !6, i64 0, !6, i64 8, !6, i64 16, !16, i64 24, !16, i64 28, !16, i64 32, !16, i64 36, !6, i64 40, !6, i64 48, !6, i64 56, !6, i64 64, !17, i64 72, !17, i64 88, !17, i64 104, !3, i64 120}
!16 = !{!"int", !3, i64 0}
!17 = !{!"timespec", !6, i64 0, !6, i64 8}
!18 = !{!19, !19, i64 0}
!19 = !{!"any pointer", !3, i64 0}
!20 = !{!21, !21, i64 0}
!21 = !{!"short", !3, i64 0}
!22 = !{!16, !16, i64 0}
!23 = !{!24, !24, i64 0}
!24 = !{!"float", !3, i64 0}
!25 = !{!26, !26, i64 0}
!26 = !{!"double", !3, i64 0}
