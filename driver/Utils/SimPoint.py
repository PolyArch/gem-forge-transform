
import ProfileMessage_pb2

import numpy
from sklearn.decomposition import PCA
from sklearn import cluster
from scipy.spatial import distance

import os

class SimPoint(object):
    def __init__(self, simpoint_id, weight):
        self.id = simpoint_id
        self.lhs_inst = 0
        self.rhs_inst = 0
        self.lhs_mark = 0
        self.rhs_mark = 0
        self.weight = weight
        # print('Find trace {weight}: {simpoint_id}'.format(
        #     weight=self.weight, simpoint_id=self.id))

    def get_weight(self):
        return self.weight

    def get_id(self):
        return self.id


def parse_simpoint_from_file(simpoint_fn):
    """
    Read in the simpoint and try to find the trace.
    Since simpoint only works for single thread workloads,
    we will always assume thread id to be zero.
    """
    simpoints = list()
    with open(simpoint_fn, 'r') as f:
        simpoint_id = 0
        for line in f:
            if line.startswith('#'):
                continue
            fields = line.split(' ')
            assert(len(fields) == 5)
            lhs_inst = int(fields[0])
            rhs_inst = int(fields[1])
            lhs_mark = int(fields[2])
            rhs_mark = int(fields[3])
            weight = float(fields[4])
            simpoint_obj = SimPoint(simpoint_id, weight)
            simpoint_obj.lhs_inst = lhs_inst
            simpoint_obj.rhs_inst = rhs_inst
            simpoint_obj.lhs_mark = lhs_mark
            simpoint_obj.rhs_mark = rhs_mark
            simpoints.append(simpoint_obj)
            simpoint_id += 1
    return simpoints


class SimPointBuilder:
    def __init__(self, fn, out_fn):
        self.fn = fn
        self.directory = os.path.dirname(fn)
        self.profile = ProfileMessage_pb2.Profile()
        print(fn)
        try:
            with open(fn) as f:
                self.profile.ParseFromString(f.read())
        except IOError:
            print('Failed to open profile file {fn}.'.format(fn=fn))
        print('Creating BBVMap...')
        self._createBBVMap()
        self.total_insts = sum([interval.inst_rhs - interval.inst_lhs
                                for interval in self.profile.intervals])
        print('Creating BBV...')
        self.BBVs = [self._createBBV(interval)
                     for interval in self.profile.intervals]
        self.BBVIntervalIdx = [i for i in range(len(self.BBVs))]
        print('Creating Weight...')
        self.weights = [self._computeIntervalWeight(interval)
                        for interval in self.profile.intervals]
        print('Merge Identical Intervals...')
        self._mergeIdenticalInterval()

        self.BBVs = numpy.array(self.BBVs)
        self.weights = numpy.array(self.weights)
        print(len(self.profile.intervals))

        print('Doing PCA...')
        if len(self.BBVIntervalIdx) <= 15:
            self._acceptAllIntervalsAsSimPoints()
        else:
            self.X_projected = self._PCA(15)
            self._kmeans(15)
            self._findSimPoints()
        self._dumpSimPoints(out_fn)

    def _createBBVMap(self):
        self.static_bb_count = 0
        self.static_bb_map = dict()
        self.id_to_static_bb_map = dict()
        for f in self.profile.funcs:
            if f not in self.static_bb_map:
                self.static_bb_map[f] = dict()
            for bb in self.profile.funcs[f].bbs:
                if bb not in self.static_bb_map[f]:
                    self.static_bb_map[f][bb] = self.static_bb_count
                    self.id_to_static_bb_map[self.static_bb_count] = (f, bb)
                    self.static_bb_count += 1

    def _createBBV(self, interval):
        BBV = [0] * self.static_bb_count
        for f in interval.funcs:
            for bb in interval.funcs[f].bbs:
                idx = self.static_bb_map[f][bb]
                cnt = interval.funcs[f].bbs[bb]
                BBV[idx] = cnt
        total = float(sum(BBV))
        BBV = [s / total for s in BBV]
        return BBV

    def _computeIntervalWeight(self, interval):
        return float(interval.inst_rhs - interval.inst_lhs) / self.total_insts

    def _mergeIdenticalInterval(self):
        merged_bbvs = list()
        merged_bbv_interval_index = list()
        merged_bbv_weights = list()
        for i in range(len(self.BBVs)):
            bbv = self.BBVs[i]
            weight = self.weights[i]
            interval_idx = self.BBVIntervalIdx[i]
            merged = False
            numpy_bbv = numpy.array(bbv)
            for j in range(len(merged_bbvs)):
                target_bbv = merged_bbvs[j]
                numpy_target_bbv = numpy.array(target_bbv)
                diff = numpy_bbv - numpy_target_bbv
                distance = numpy.linalg.norm(diff)
                if distance < 1e-9:
                    # Merge the weight.
                    merged_bbv_weights[j] += weight
                    merged = True
                    break
            if not merged:
                merged_bbvs.append(bbv)
                merged_bbv_weights.append(weight)
                merged_bbv_interval_index.append(interval_idx)
        print('Merge {x} intervals into {y}'.format(
            x=len(self.BBVs),
            y=len(merged_bbvs),
        ))
        self.BBVs = merged_bbvs
        self.BBVIntervalIdx = merged_bbv_interval_index
        self.weights = merged_bbv_weights

    def _PCA(self, dim):
        X = self.BBVs
        pca = PCA(n_components=dim)
        pca.fit(X)
        X_projected = pca.transform(X)
        print(pca.explained_variance_ratio_)
        print(pca.singular_values_)
        return X_projected

    def _computeBIC(self, kmeans, X, sample_weight):
        """
        Computes the BIC metric for a given clusters

        Parameters:
        -----------------------------------------
        kmeans:  List of clustering object from scikit learn

        X     :  multidimension np array of data points

        Returns:
        -----------------------------------------
        BIC value
        """
        # assign centers and labels
        centers = [kmeans.cluster_centers_]
        labels = kmeans.labels_
        # number of clusters
        m = kmeans.n_clusters
        # size of the clusters
        n = numpy.bincount(labels)
        # size of data set
        N, d = X.shape

        # cluster weight is the sum of weights in each cluster
        cluster_weight = [sum(sample_weight[numpy.where(labels == i)])
                          for i in range(m)]

        # compute variance for all clusters beforehand
        distance_square = [
            sum(distance.cdist(
                X[numpy.where(labels == i)], [centers[0][i]], 'euclidean')**2
                ) for i in range(m)]
        cluster_variance = (1.0 / (N - m) / d) * sum(distance_square)
        distance_square_weight = list()
        for i in range(m):
            index = numpy.where(labels == i)
            Xs = X[index]
            weights = numpy.reshape(sample_weight[index], (-1, 1))
            d2 = distance.cdist(Xs, [centers[0][i]], 'euclidean') ** 2
            weighted_distance = numpy.multiply(d2, weights)
            distance_square_weight.append(sum(weighted_distance))
        cluster_variance_weight = (
            1.0 / (N - m) / d) * sum(distance_square_weight)

        const_term = 0.5 * m * numpy.log(N) * (d + 1)

        BIC_weight = numpy.sum([N * cluster_weight[i] * numpy.log(cluster_weight[i]) -
                                (N * cluster_weight[i] * d / 2) * numpy.log(2 * numpy.pi * cluster_variance) -
                                ((N * cluster_weight[i] - 1) * d / 2)
                                for i in range(m)]) - const_term

        BIC = numpy.sum([n[i] * (numpy.log(n[i]) - numpy.log(N)) -
                         ((n[i] * d) / 2) * numpy.log(2 * numpy.pi * cluster_variance) -
                         ((n[i] - 1) * d / 2) for i in range(m)]) - const_term
        return BIC_weight

    def _kmeans(self, max_k):
        self.BICs = list()
        self.ys = list()
        self.centers = list()
        for n_cluster in xrange(1, max_k):
            kmeans = cluster.KMeans(n_clusters=n_cluster)
            kmeans.fit(self.X_projected, sample_weight=self.weights)
            BIC = self._computeBIC(
                kmeans, self.X_projected, sample_weight=self.weights)
            self.BICs.append(BIC)
            print('k={k}, BIC={BIC}'.format(k=n_cluster, BIC=BIC))
            y = kmeans.predict(self.X_projected, sample_weight=self.weights)
            self.ys.append(y)
            self.centers.append(kmeans.cluster_centers_)

    def _plotKMeans(self):
        # KMeans
        import matplotlib
        matplotlib.use('agg')
        import matplotlib.pyplot as plt
        for y in self.ys:
            n = y.shape[0]
            y_sim = numpy.ones([n, n])
            for i in xrange(n):
                for j in xrange(n):
                    if y[i] == y[j]:
                        y_sim[i, j] = y[i] + 1  # Make the label starts from 1.
            plt.matshow(y_sim[0:100, 0:100])
            plt.savefig(os.path.join(self.directory,
                                     'kmeans-{k}.pdf'.format(k=n_cluster)))

    def _findSimPoints(self):
        print('findSimPoints')
        threshold = min(self.BICs) + 0.8 * (max(self.BICs) - min(self.BICs))
        self.chosen_k = 0
        for k in xrange(len(self.BICs)):
            if self.BICs[k] > threshold:
                self.chosen_k = k
                break
        print('Chosen k = {k}.'.format(k=self.chosen_k))
        y = self.ys[self.chosen_k]
        centers = self.centers[self.chosen_k]
        min_distances = [float('inf')] * (self.chosen_k + 1)

        self.simpoints = [-1] * (self.chosen_k + 1)
        for idx in range(len(y)):
            group_id = y[idx]
            distance = numpy.linalg.norm(
                self.X_projected[idx] - centers[group_id])
            if distance < min_distances[group_id]:
                min_distances[group_id] = distance
                self.simpoints[group_id] = idx
        for p in self.simpoints:
            assert(p != -1)
        self.simpoints_label = numpy.argsort(self.simpoints)
        self.simpoints.sort()

        unique, counts = numpy.unique(y, return_counts=True)
        self.simpoint_weights = dict(
            zip(unique, [sum(self.weights[numpy.where(y == i)]) for i in unique]))

    def _acceptAllIntervalsAsSimPoints(self):
        print('accept all {x} intervals as simpoints'.format(x=len(self.BBVIntervalIdx)))
        self.simpoints = [i for i in range(len(self.BBVIntervalIdx))]
        self.simpoint_weights = [w for w in self.weights]
        self.simpoints_label = [i for i in range(len(self.BBVIntervalIdx))]

    def _dumpSimPoints(self, out_fn):
        print('dumpSimPoints to {out_fn}.'.format(out_fn=out_fn))
        points = open(out_fn, 'w')

        # First dump real function profile.
        self._dumpRealFuncProfile(points)
        self._dumpEstimatedFuncProfile(points)

        for i in range(len(self.simpoints)):
            simpoint_bbv = self.simpoints[i]
            simpoint_interval = self.BBVIntervalIdx[simpoint_bbv]
            simpoint_label = self.simpoints_label[i]
            simpoint_weight = self.simpoint_weights[simpoint_label]

            interval = self.profile.intervals[simpoint_interval]

            # Raw file for program to read.
            points.write('{lhs} {rhs} {lhs_mark} {rhs_mark} {weight}\n'.format(
                lhs=interval.inst_lhs, rhs=interval.inst_rhs,
                lhs_mark=interval.mark_lhs, rhs_mark=interval.mark_rhs,
                weight=simpoint_weight))
            # for func in interval.funcs:
            #     points.write('- {func}\n'.format(func=func))
            points.write('# =========== SimPoint #{i} [{lhs} {rhs}) [{lhs_mark} {rhs_mark}) ============ \n'.format(
                i=i, lhs=interval.inst_lhs, rhs=interval.inst_rhs,
                lhs_mark=interval.mark_lhs, rhs_mark=interval.mark_rhs,
            ))

            BBV = self.BBVs[simpoint_bbv]
            sorted_bb_idx = numpy.argsort(-BBV)
            summed = 0.0
            for bb_idx in sorted_bb_idx:
                f, bb = self.id_to_static_bb_map[bb_idx]
                summed += BBV[bb_idx]
                points.write('# {f} {bb} {acc} {v}.\n'.format(
                    f=f, bb=bb, acc=summed, v=BBV[bb_idx]))
                if summed > 0.99:
                    break
        points.close()

    def _dumpRealFuncProfile(self, f):
        # First compute function instruction count.
        func_inst_count = dict()
        func_list = list()
        total_inst_count = 0
        for func in self.profile.funcs:
            inst_count = 0
            for bb in self.profile.funcs[func].bbs:
                inst_count += self.profile.funcs[func].bbs[bb]
            func_inst_count[func] = inst_count
            total_inst_count += inst_count
            func_list.append(func)

        # Sort with descending instruction count
        func_list.sort(key=lambda func: func_inst_count[func], reverse=True)
        accumulated = 0.0
        threshold = 0.99
        self.real_func_profile = list()
        f.write('# ================== Real Func Profile ================\n')
        for func in func_list:
            percentage = float(func_inst_count[func]) / float(total_inst_count)
            f.write('# {acc:0.4f} {percentage:0.4f} {func}\n'.format(
                acc=accumulated, percentage=percentage, func=func,
            ))
            accumulated += percentage
            self.real_func_profile.append((func, percentage))
            if accumulated > threshold:
                break

    def _dumpEstimatedFuncProfile(self, f):
        """
        Dump the function profile estimated from the simpoint.
        """

        func_inst_count = dict()
        total_inst_count = 0
        for i in range(len(self.simpoints)):
            simpoint_bbv = self.simpoints[i]
            simpoint_interval = self.BBVIntervalIdx[simpoint_bbv]
            simpoint_label = self.simpoints_label[i]
            simpoint_weight = self.simpoint_weights[simpoint_label]

            interval = self.profile.intervals[simpoint_interval]
            interval_insts = interval.inst_rhs - interval.inst_lhs
            interval_weight = float(interval_insts) / float(self.total_insts)
            for func in interval.funcs:
                inst_count = 0
                for bb in interval.funcs[func].bbs:
                    inst_count += interval.funcs[func].bbs[bb]
                # Weighted by this interval's weight.
                inst_count = inst_count * simpoint_weight / interval_weight
                if func not in func_inst_count:
                    func_inst_count[func] = inst_count
                else:
                    func_inst_count[func] += inst_count
                total_inst_count += inst_count

        func_list = list(func_inst_count.keys())

        # Sort with descending instruction count
        func_list.sort(key=lambda func: func_inst_count[func], reverse=True)
        accumulated = 0.0
        threshold = 0.99
        self.estimated_func_profile = list()
        f.write('# ================== Estimated Func Profile ================\n')
        for func in func_list:
            percentage = float(func_inst_count[func]) / float(total_inst_count)
            f.write('# {acc:0.4f} {percentage:0.4f} {func}\n'.format(
                acc=accumulated, percentage=percentage, func=func,
            ))
            self.estimated_func_profile.append((func, percentage))
            accumulated += percentage
            if accumulated > threshold:
                break
        
        # Check for our estimation.
        accumulated = 0.0
        threshold = 0.9
        for i in range(len(self.real_func_profile)):
            real_func = self.real_func_profile[i][0]
            real_percentage = self.real_func_profile[i][1]
            if i >= len(self.estimated_func_profile):
                print('\033[93mWarn: Missing EstimatedFuncProfile\033[0m')
                break
            est_func = self.estimated_func_profile[i][0]
            est_percentage = self.estimated_func_profile[i][1]
            if real_func != est_func or abs(real_percentage - est_percentage) > 0.01:
                print('\033[93mWarn: Mismatch Real {real} {v_real} != Estmated {est} {v_est}\033[0m'.format(
                    real=real_func, v_real=real_percentage,
                    est=est_func, v_est=est_percentage,
                ))
            accumulated += real_percentage
            if accumulated > threshold:
                break
