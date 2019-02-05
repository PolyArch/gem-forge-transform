
import ProfileMessage_pb2

import numpy
from sklearn.decomposition import PCA
from sklearn import cluster
from scipy.spatial import distance

import os


class SimPoint:
    def __init__(self, fn):
        self.fn = fn
        self.directory = os.path.dirname(fn)
        self.profile = ProfileMessage_pb2.Profile()
        try:
            with open(fn) as f:
                self.profile.ParseFromString(f.read())
        except IOError:
            print('Failed to open profile file {fn}.'.format(fn=fn))
        print('Creating BBVMap...')
        self._createBBVMap()
        self.BBVs = [self._createBBV(interval)
                     for interval in self.profile.intervals]
        self.BBVs = numpy.array(self.BBVs)

        print('Doing PCA...')
        self.X_projected = self._PCA(15)
        self._kmeans(15)
        self._findSimPoints()
        self._dumpSimPoints()

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

    def _PCA(self, dim):
        X = self.BBVs
        pca = PCA(n_components=dim)
        pca.fit(X)
        X_projected = pca.transform(X)
        print(pca.explained_variance_ratio_)
        print(pca.singular_values_)
        return X_projected

    def _computeBIC(self, kmeans, X):
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

        # compute variance for all clusters beforehand
        cl_var = (1.0 / (N - m) / d) * sum([sum(distance.cdist(X[numpy.where(labels == i)], [centers[0][i]],
                                                               'euclidean')**2) for i in range(m)])

        const_term = 0.5 * m * numpy.log(N) * (d + 1)

        BIC = numpy.sum([n[i] * numpy.log(n[i]) -
                         n[i] * numpy.log(N) -
                         ((n[i] * d) / 2) * numpy.log(2 * numpy.pi * cl_var) -
                         ((n[i] - 1) * d / 2) for i in range(m)]) - const_term

        return BIC

    def _kmeans(self, max_k):
        self.BICs = list()
        self.ys = list()
        self.centers = list()
        for n_cluster in xrange(1, max_k):
            kmeans = cluster.KMeans(n_clusters=n_cluster)
            kmeans.fit(self.X_projected)
            BIC = self._computeBIC(kmeans, self.X_projected)
            self.BICs.append(BIC)
            print('k={k}, BIC={BIC}'.format(k=n_cluster, BIC=BIC))
            y = kmeans.predict(self.X_projected)
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
        self.weights = dict(
            zip(unique, [v / float(sum(counts)) for v in counts]))

    def _dumpSimPoints(self):
        print('dumpSimPoints')
        points = open(os.path.join(self.directory, 'simpoints.txt'), 'w')

        for i in xrange(len(self.simpoints)):
            simpoint = self.simpoints[i]
            simpoint_label = self.simpoints_label[i]
            simpoint_weight = self.weights[simpoint_label]

            interval = self.profile.intervals[simpoint]

            # Raw file for program to read.
            points.write('{lhs} {rhs} {weight}\n'.format(
                lhs=interval.inst_lhs, rhs=interval.inst_rhs, weight=simpoint_weight))
            points.write('# ============SimPoint #{i} [{lhs} {rhs}]============ \n'.format(
                i=i, lhs=interval.inst_lhs, rhs=interval.inst_rhs))

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

    def _dumpMatrix(self, mat, fn):
        with open(fn, 'w') as f:
            for row in mat:
                for v in row:
                    f.write('{v},'.format(v))
                f.write('\n')
