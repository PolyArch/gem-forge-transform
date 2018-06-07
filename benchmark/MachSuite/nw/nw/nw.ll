; ModuleID = 'nw.c'
source_filename = "nw.c"
target datalayout = "e-m:e-i64:64-f80:128-n8:16:32:64-S128"
target triple = "x86_64-unknown-linux-gnu"

; Function Attrs: norecurse nounwind uwtable
define void @needwun(i8* nocapture readonly %SEQA, i8* nocapture readonly %SEQB, i8* nocapture %alignedA, i8* nocapture %alignedB, i32* nocapture %M, i8* nocapture %ptr) local_unnamed_addr #0 {
entry:
  br label %for.body

for.body:                                         ; preds = %for.body, %entry
  %indvars.iv277 = phi i64 [ 0, %entry ], [ %indvars.iv.next278, %for.body ]
  %arrayidx = getelementptr inbounds i32, i32* %M, i64 %indvars.iv277
  %0 = trunc i64 %indvars.iv277 to i32
  %1 = sub i32 0, %0
  store i32 %1, i32* %arrayidx, align 4, !tbaa !2
  %indvars.iv.next278 = add nuw nsw i64 %indvars.iv277, 1
  %exitcond280 = icmp eq i64 %indvars.iv.next278, 129
  br i1 %exitcond280, label %for.body3.preheader, label %for.body

for.body3.preheader:                              ; preds = %for.body
  br label %for.body3

for.body3:                                        ; preds = %for.body3.preheader, %for.body3
  %indvars.iv272 = phi i64 [ %indvars.iv.next273, %for.body3 ], [ 0, %for.body3.preheader ]
  %2 = mul nuw nsw i64 %indvars.iv272, 129
  %arrayidx7 = getelementptr inbounds i32, i32* %M, i64 %2
  %3 = trunc i64 %indvars.iv272 to i32
  %4 = sub i32 0, %3
  store i32 %4, i32* %arrayidx7, align 4, !tbaa !2
  %indvars.iv.next273 = add nuw nsw i64 %indvars.iv272, 1
  %exitcond276 = icmp eq i64 %indvars.iv.next273, 129
  br i1 %exitcond276, label %for.body16.lver.check.preheader, label %for.body3

for.body16.lver.check.preheader:                  ; preds = %for.body3
  br label %for.body16.lver.check

for.body16.lver.check:                            ; preds = %for.body16.lver.check.preheader, %for.inc80
  %indvar = phi i64 [ %indvar.next, %for.inc80 ], [ 0, %for.body16.lver.check.preheader ]
  %indvars.iv266 = phi i64 [ %indvars.iv.next267, %for.inc80 ], [ 1, %for.body16.lver.check.preheader ]
  %5 = mul i64 %indvar, 129
  %scevgep281 = getelementptr i32, i32* %M, i64 %5
  %scevgep281282 = bitcast i32* %scevgep281 to i8*
  %6 = add i64 %5, 258
  %scevgep283 = getelementptr i32, i32* %M, i64 %6
  %scevgep283284 = bitcast i32* %scevgep283 to i8*
  %7 = add i64 %5, 130
  %scevgep285 = getelementptr i8, i8* %ptr, i64 %7
  %scevgep286 = getelementptr i8, i8* %ptr, i64 %6
  %8 = add nsw i64 %indvars.iv266, -1
  %arrayidx21 = getelementptr inbounds i8, i8* %SEQB, i64 %8
  %9 = mul nuw nsw i64 %8, 129
  %10 = mul nuw nsw i64 %indvars.iv266, 129
  %bound0 = icmp ugt i8* %scevgep286, %scevgep281282
  %bound1 = icmp ult i8* %scevgep285, %scevgep283284
  %memcheck.conflict = and i1 %bound0, %bound1
  br i1 %memcheck.conflict, label %for.body16.lver.orig.preheader, label %for.body16.ph

for.body16.lver.orig.preheader:                   ; preds = %for.body16.lver.check
  br label %for.body16.lver.orig

for.body16.lver.orig:                             ; preds = %for.body16.lver.orig.preheader, %for.inc77.lver.orig
  %indvars.iv259.lver.orig = phi i64 [ %indvars.iv.next260.lver.orig, %for.inc77.lver.orig ], [ 1, %for.body16.lver.orig.preheader ]
  %11 = add nsw i64 %indvars.iv259.lver.orig, -1
  %arrayidx18.lver.orig = getelementptr inbounds i8, i8* %SEQA, i64 %11
  %12 = load i8, i8* %arrayidx18.lver.orig, align 1, !tbaa !6
  %13 = load i8, i8* %arrayidx21, align 1, !tbaa !6
  %cmp23.lver.orig = icmp eq i8 %12, %13
  %..lver.orig = select i1 %cmp23.lver.orig, i32 1, i32 -1
  %14 = add nuw nsw i64 %11, %9
  %arrayidx30.lver.orig = getelementptr inbounds i32, i32* %M, i64 %14
  %15 = load i32, i32* %arrayidx30.lver.orig, align 4, !tbaa !2
  %add31.lver.orig = add nsw i32 %..lver.orig, %15
  %16 = add nuw nsw i64 %indvars.iv259.lver.orig, %9
  %arrayidx34.lver.orig = getelementptr inbounds i32, i32* %M, i64 %16
  %17 = load i32, i32* %arrayidx34.lver.orig, align 4, !tbaa !2
  %add35.lver.orig = add nsw i32 %17, -1
  %18 = add nuw nsw i64 %11, %10
  %arrayidx39.lver.orig = getelementptr inbounds i32, i32* %M, i64 %18
  %19 = load i32, i32* %arrayidx39.lver.orig, align 4, !tbaa !2
  %add40.lver.orig = add nsw i32 %19, -1
  %cmp41.lver.orig = icmp sgt i32 %add35.lver.orig, %add40.lver.orig
  %cond.lver.orig = select i1 %cmp41.lver.orig, i32 %add35.lver.orig, i32 %add40.lver.orig
  %cmp43.lver.orig = icmp sgt i32 %add31.lver.orig, %cond.lver.orig
  %cond54.lver.orig = select i1 %cmp43.lver.orig, i32 %add31.lver.orig, i32 %cond.lver.orig
  %20 = add nuw nsw i64 %indvars.iv259.lver.orig, %10
  %arrayidx57.lver.orig = getelementptr inbounds i32, i32* %M, i64 %20
  store i32 %cond54.lver.orig, i32* %arrayidx57.lver.orig, align 4, !tbaa !2
  %cmp58.lver.orig = icmp eq i32 %cond54.lver.orig, %add40.lver.orig
  br i1 %cmp58.lver.orig, label %if.then60.lver.orig, label %if.else64.lver.orig

if.else64.lver.orig:                              ; preds = %for.body16.lver.orig
  %cmp65.lver.orig = icmp eq i32 %cond54.lver.orig, %add35.lver.orig
  %arrayidx70.lver.orig = getelementptr inbounds i8, i8* %ptr, i64 %20
  br i1 %cmp65.lver.orig, label %if.then67.lver.orig, label %if.else71.lver.orig

if.else71.lver.orig:                              ; preds = %if.else64.lver.orig
  store i8 92, i8* %arrayidx70.lver.orig, align 1, !tbaa !6
  br label %for.inc77.lver.orig

if.then67.lver.orig:                              ; preds = %if.else64.lver.orig
  store i8 94, i8* %arrayidx70.lver.orig, align 1, !tbaa !6
  br label %for.inc77.lver.orig

if.then60.lver.orig:                              ; preds = %for.body16.lver.orig
  %arrayidx63.lver.orig = getelementptr inbounds i8, i8* %ptr, i64 %20
  store i8 60, i8* %arrayidx63.lver.orig, align 1, !tbaa !6
  br label %for.inc77.lver.orig

for.inc77.lver.orig:                              ; preds = %if.then60.lver.orig, %if.then67.lver.orig, %if.else71.lver.orig
  %indvars.iv.next260.lver.orig = add nuw nsw i64 %indvars.iv259.lver.orig, 1
  %exitcond.lver.orig = icmp eq i64 %indvars.iv.next260.lver.orig, 129
  br i1 %exitcond.lver.orig, label %for.inc80, label %for.body16.lver.orig

for.body16.ph:                                    ; preds = %for.body16.lver.check
  %21 = mul i64 %indvar, 129
  %22 = add i64 %21, 129
  %scevgep287 = getelementptr i32, i32* %M, i64 %22
  %load_initial = load i32, i32* %scevgep287, align 4
  br label %for.body16

for.body16:                                       ; preds = %for.inc77, %for.body16.ph
  %store_forwarded = phi i32 [ %load_initial, %for.body16.ph ], [ %cond54, %for.inc77 ]
  %indvars.iv259 = phi i64 [ 1, %for.body16.ph ], [ %indvars.iv.next260, %for.inc77 ]
  %23 = add nsw i64 %indvars.iv259, -1
  %arrayidx18 = getelementptr inbounds i8, i8* %SEQA, i64 %23
  %24 = load i8, i8* %arrayidx18, align 1, !tbaa !6
  %25 = load i8, i8* %arrayidx21, align 1, !tbaa !6
  %cmp23 = icmp eq i8 %24, %25
  %. = select i1 %cmp23, i32 1, i32 -1
  %26 = add nuw nsw i64 %23, %9
  %arrayidx30 = getelementptr inbounds i32, i32* %M, i64 %26
  %27 = load i32, i32* %arrayidx30, align 4, !tbaa !2
  %add31 = add nsw i32 %., %27
  %28 = add nuw nsw i64 %indvars.iv259, %9
  %arrayidx34 = getelementptr inbounds i32, i32* %M, i64 %28
  %29 = load i32, i32* %arrayidx34, align 4, !tbaa !2
  %add35 = add nsw i32 %29, -1
  %add40 = add nsw i32 %store_forwarded, -1
  %cmp41 = icmp sgt i32 %add35, %add40
  %cond = select i1 %cmp41, i32 %add35, i32 %add40
  %cmp43 = icmp sgt i32 %add31, %cond
  %cond54 = select i1 %cmp43, i32 %add31, i32 %cond
  %30 = add nuw nsw i64 %indvars.iv259, %10
  %arrayidx57 = getelementptr inbounds i32, i32* %M, i64 %30
  store i32 %cond54, i32* %arrayidx57, align 4, !tbaa !2
  %cmp58 = icmp eq i32 %cond54, %add40
  br i1 %cmp58, label %if.then60, label %if.else64

if.then60:                                        ; preds = %for.body16
  %arrayidx63 = getelementptr inbounds i8, i8* %ptr, i64 %30
  store i8 60, i8* %arrayidx63, align 1, !tbaa !6
  br label %for.inc77

if.else64:                                        ; preds = %for.body16
  %cmp65 = icmp eq i32 %cond54, %add35
  %arrayidx70 = getelementptr inbounds i8, i8* %ptr, i64 %30
  br i1 %cmp65, label %if.then67, label %if.else71

if.then67:                                        ; preds = %if.else64
  store i8 94, i8* %arrayidx70, align 1, !tbaa !6
  br label %for.inc77

if.else71:                                        ; preds = %if.else64
  store i8 92, i8* %arrayidx70, align 1, !tbaa !6
  br label %for.inc77

for.inc77:                                        ; preds = %if.then60, %if.else71, %if.then67
  %indvars.iv.next260 = add nuw nsw i64 %indvars.iv259, 1
  %exitcond = icmp eq i64 %indvars.iv.next260, 129
  br i1 %exitcond, label %for.inc80, label %for.body16

for.inc80:                                        ; preds = %for.inc77, %for.inc77.lver.orig
  %indvars.iv.next267 = add nuw nsw i64 %indvars.iv266, 1
  %exitcond271 = icmp eq i64 %indvars.iv.next267, 129
  %indvar.next = add i64 %indvar, 1
  br i1 %exitcond271, label %while.body.preheader, label %for.body16.lver.check

while.body.preheader:                             ; preds = %for.inc80
  br label %while.body

while.body:                                       ; preds = %while.body.preheader, %if.end138
  %indvars.iv = phi i64 [ %indvars.iv.next, %if.end138 ], [ 0, %while.body.preheader ]
  %b_idx.2251 = phi i32 [ %b_idx.3, %if.end138 ], [ 128, %while.body.preheader ]
  %a_idx.2250 = phi i32 [ %a_idx.3, %if.end138 ], [ 128, %while.body.preheader ]
  %mul87 = mul nsw i32 %b_idx.2251, 129
  %add88 = add nsw i32 %mul87, %a_idx.2250
  %idxprom89 = sext i32 %add88 to i64
  %arrayidx90 = getelementptr inbounds i8, i8* %ptr, i64 %idxprom89
  %31 = load i8, i8* %arrayidx90, align 1, !tbaa !6
  switch i8 %31, label %if.else126 [
    i8 92, label %if.then94
    i8 60, label %if.then115
  ]

if.then94:                                        ; preds = %while.body
  %sub95 = add nsw i32 %a_idx.2250, -1
  %idxprom96 = sext i32 %sub95 to i64
  %arrayidx97 = getelementptr inbounds i8, i8* %SEQA, i64 %idxprom96
  %32 = load i8, i8* %arrayidx97, align 1, !tbaa !6
  %arrayidx100 = getelementptr inbounds i8, i8* %alignedA, i64 %indvars.iv
  store i8 %32, i8* %arrayidx100, align 1, !tbaa !6
  %sub101 = add nsw i32 %b_idx.2251, -1
  %idxprom102 = sext i32 %sub101 to i64
  %arrayidx103 = getelementptr inbounds i8, i8* %SEQB, i64 %idxprom102
  %33 = load i8, i8* %arrayidx103, align 1, !tbaa !6
  br label %if.end138

if.then115:                                       ; preds = %while.body
  %sub116 = add nsw i32 %a_idx.2250, -1
  %idxprom117 = sext i32 %sub116 to i64
  %arrayidx118 = getelementptr inbounds i8, i8* %SEQA, i64 %idxprom117
  %34 = load i8, i8* %arrayidx118, align 1, !tbaa !6
  %arrayidx121 = getelementptr inbounds i8, i8* %alignedA, i64 %indvars.iv
  store i8 %34, i8* %arrayidx121, align 1, !tbaa !6
  br label %if.end138

if.else126:                                       ; preds = %while.body
  %arrayidx129 = getelementptr inbounds i8, i8* %alignedA, i64 %indvars.iv
  store i8 45, i8* %arrayidx129, align 1, !tbaa !6
  %sub130 = add nsw i32 %b_idx.2251, -1
  %idxprom131 = sext i32 %sub130 to i64
  %arrayidx132 = getelementptr inbounds i8, i8* %SEQB, i64 %idxprom131
  %35 = load i8, i8* %arrayidx132, align 1, !tbaa !6
  br label %if.end138

if.end138:                                        ; preds = %if.then115, %if.else126, %if.then94
  %.sink = phi i8 [ 45, %if.then115 ], [ %35, %if.else126 ], [ %33, %if.then94 ]
  %a_idx.3 = phi i32 [ %sub116, %if.then115 ], [ %a_idx.2250, %if.else126 ], [ %sub95, %if.then94 ]
  %b_idx.3 = phi i32 [ %b_idx.2251, %if.then115 ], [ %sub130, %if.else126 ], [ %sub101, %if.then94 ]
  %arrayidx124 = getelementptr inbounds i8, i8* %alignedB, i64 %indvars.iv
  store i8 %.sink, i8* %arrayidx124, align 1, !tbaa !6
  %indvars.iv.next = add nuw i64 %indvars.iv, 1
  %cmp83 = icmp sgt i32 %a_idx.3, 0
  %cmp85 = icmp sgt i32 %b_idx.3, 0
  %36 = or i1 %cmp83, %cmp85
  br i1 %36, label %while.body, label %pad_a

pad_a:                                            ; preds = %if.end138
  %37 = trunc i64 %indvars.iv.next to i32
  %38 = trunc i64 %indvars.iv.next to i32
  %cmp140248 = icmp ult i32 %37, 256
  br i1 %cmp140248, label %pad_b.loopexit, label %pad_b

pad_b.loopexit:                                   ; preds = %pad_a
  %39 = and i64 %indvars.iv.next, 4294967295
  %scevgep258 = getelementptr i8, i8* %alignedA, i64 %39
  %40 = sub i64 254, %indvars.iv
  %41 = and i64 %40, 4294967295
  %42 = add nuw nsw i64 %41, 1
  call void @llvm.memset.p0i8.i64(i8* %scevgep258, i8 95, i64 %42, i32 1, i1 false)
  br label %pad_b

pad_b:                                            ; preds = %pad_b.loopexit, %pad_a
  %cmp149246 = icmp ult i32 %38, 256
  br i1 %cmp149246, label %for.end156.loopexit, label %for.end156

for.end156.loopexit:                              ; preds = %pad_b
  %43 = and i64 %indvars.iv.next, 4294967295
  %scevgep = getelementptr i8, i8* %alignedB, i64 %43
  %44 = sub i64 254, %indvars.iv
  %45 = and i64 %44, 4294967295
  %46 = add nuw nsw i64 %45, 1
  call void @llvm.memset.p0i8.i64(i8* %scevgep, i8 95, i64 %46, i32 1, i1 false)
  br label %for.end156

for.end156:                                       ; preds = %for.end156.loopexit, %pad_b
  ret void
}

; Function Attrs: argmemonly nounwind
declare void @llvm.memset.p0i8.i64(i8* nocapture writeonly, i8, i64, i32, i1) #1

attributes #0 = { norecurse nounwind uwtable "correctly-rounded-divide-sqrt-fp-math"="false" "disable-tail-calls"="false" "less-precise-fpmad"="false" "no-frame-pointer-elim"="false" "no-infs-fp-math"="false" "no-jump-tables"="false" "no-nans-fp-math"="false" "no-signed-zeros-fp-math"="false" "no-trapping-math"="false" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+fxsr,+mmx,+sse,+sse2,+x87" "unsafe-fp-math"="false" "use-soft-float"="false" }
attributes #1 = { argmemonly nounwind }

!llvm.module.flags = !{!0}
!llvm.ident = !{!1}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{!"clang version 6.0.0 (git@github.com:seanzw/clang.git bb8d45f8ab88237f1fa0530b8ad9b96bf4a5e6cc) (git@github.com:seanzw/llvm.git 16ebb58ea40d384e8daa4c48d2bf7dd1ccfa5fcd)"}
!2 = !{!3, !3, i64 0}
!3 = !{!"int", !4, i64 0}
!4 = !{!"omnipotent char", !5, i64 0}
!5 = !{!"Simple C/C++ TBAA"}
!6 = !{!4, !4, i64 0}
