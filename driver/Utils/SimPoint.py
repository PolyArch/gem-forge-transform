
import ProfileMessage_pb2

import numpy
from sklearn.decomposition import PCA
from sklearn import cluster
from scipy.spatial import distance

import os


class SimPoint:
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
        print('Creating Weight...')
        self.weights = [self._computeIntervalWeight(interval)
                        for interval in self.profile.intervals]

        self.BBVs = numpy.array(self.BBVs)
        self.weights = numpy.array(self.weights)
        print(len(self.profile.intervals))

        print('Doing PCA...')
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
        for interval in xrange(len(self.profile.intervals)):
            k = y[interval]
            distance = numpy.linalg.norm(
                self.X_projected[interval] - centers[k])
            if distance < min_distances[k]:
                min_distances[k] = distance
                self.simpoints[k] = interval
        for p in self.simpoints:
            assert(p != -1)
        self.simpoints_label = numpy.argsort(self.simpoints)
        self.simpoints.sort()

        unique, counts = numpy.unique(y, return_counts=True)
        self.simpoint_weights = dict(
            zip(unique, [sum(self.weights[numpy.where(y == i)]) for i in unique]))

    def _dumpSimPoints(self, out_fn):
        print('dumpSimPoints to {out_fn}.'.format(out_fn=out_fn))
        points = open(out_fn, 'w')

        # First dump real function profile.
        self._dumpRealFuncProfile(points)
        self._dumpEstimatedFuncProfile(points)

        for i in range(len(self.simpoints)):
            simpoint = self.simpoints[i]
            simpoint_label = self.simpoints_label[i]
            simpoint_weight = self.simpoint_weights[simpoint_label]

            interval = self.profile.intervals[simpoint]

            # Raw file for program to read.
            points.write('{lhs} {rhs} {lhs_mark} {rhs_mark} {weight}\n'.format(
                lhs=interval.inst_lhs, rhs=interval.inst_rhs,
                lhs_mark=interval.mark_lhs, rhs_mark=interval.mark_rhs,
                weight=simpoint_weight))
            points.write('# =========== SimPoint #{i} [{lhs} {rhs}) [{lhs_mark} {rhs_mark}) ============ \n'.format(
                i=i, lhs=interval.inst_lhs, rhs=interval.inst_rhs,
                lhs_mark=interval.mark_lhs, rhs_mark=interval.mark_rhs,
            ))

            BBV = self.BBVs[simpoint]
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
        f.write('# ================== Real Func Profile ================\n')
        for func in func_list:
            percentage = float(func_inst_count[func]) / float(total_inst_count)
            f.write('# {acc:0.4f} {percentage:0.4f} {func}\n'.format(
                acc=accumulated, percentage=percentage, func=func,
            ))
            accumulated += percentage
            if accumulated > threshold:
                break

    def _dumpEstimatedFuncProfile(self, f):
        """
        Dump the function profile estimated from the simpoint.
        """

        func_inst_count = dict()
        total_inst_count = 0
        for i in range(len(self.simpoints)):
            simpoint = self.simpoints[i]
            simpoint_label = self.simpoints_label[i]
            simpoint_weight = self.simpoint_weights[simpoint_label]
            sample_weight = self.weights[simpoint]

            interval = self.profile.intervals[simpoint]
            for func in interval.funcs:
                inst_count = 0
                for bb in interval.funcs[func].bbs:
                    inst_count += interval.funcs[func].bbs[bb]
                # Weighted by this interval's weight.
                inst_count = inst_count * simpoint_weight / sample_weight
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
        f.write('# ================== Estimated Func Profile ================\n')
        for func in func_list:
            percentage = float(func_inst_count[func]) / float(total_inst_count)
            f.write('# {acc:0.4f} {percentage:0.4f} {func}\n'.format(
                acc=accumulated, percentage=percentage, func=func,
            ))
            accumulated += percentage
            if accumulated > threshold:
                break
