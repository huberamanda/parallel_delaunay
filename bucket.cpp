# include "bucket.hpp"

shared_ptr<Bucket> Bucket::bb = move(shared_ptr<Bucket>((Bucket*) new BoundaryBucket()));
shared_ptr<Bucket> Bucket::root = nullptr;
int Bucket::P=0 , Bucket::N=0, Bucket::total_points = 0;
double Bucket::buffer[MAX_PNTS*2+1];

bool evenstep = false;

inline int inc(int dir, int incr){
    if (incr < 0) throw runtime_error("invalid increment in inc, must be postive");
    return (dir+incr)%8;
}

inline int Bucket::dist(const shared_ptr<Bucket> other) const {
    return (ind_i-other->i())*(ind_i-other->i()) + 
        (ind_j-other->j())*(ind_j-other->j());
}

inline void Bucket::printNeighbours() const {
    cout << "neighbours of bucket ("<<ind_i<<","<<ind_j<<"): ";
    for (auto bucket : neighbours){
        if (bucket != nullptr) {
            if (bucket->isBnd()) continue;
            cout << " ("<<bucket->ind_i<<","<<bucket->ind_j<<") ";
        }
    }
    cout << endl;
}

inline bool Bucket::indexOutOfBnds(int ii, int jj) const {
    return (ii < 0 || ii > N-1 || jj < 0 || jj > N-1);
}

void Bucket::getIndex(int const dir, int& i, int& j){
    /* compute global indices for new bucket */
    i = ind_i;
    j = ind_j;
    if (0<dir && dir<4) j++;
    if (4<dir) j--;
    if (2<dir && dir<6) i--;
    if (2>dir || dir>6) i++;
}

void Bucket::addBucket(int dir){ // to be called from corner

    if (dir<0 || dir>7) throw ("invalid direction in addBucket");
    if (neighbours[dir] != nullptr) {
        // cout << "Neighbour already exists" << endl;
        return;
    }

    /* compute global indices for new bucket */
    int i,j;
    getIndex(dir, i,j);

    if (indexOutOfBnds(i,j)) {
        // cout << root->r() << ": Indices ("<<i<<","<<j<<") out of bound in addBucket. No Bucket added" << endl;
        return;
    }
    // cout << "add Bucket with global indices ("<<i<<","<<j<<") in dir " << dir << endl;
    
    int cd = dir; // current direction of new Bucket (viewpoint current bucket)
    int next_dir; // dir to go 
    shared_ptr<Bucket> current = self;
    /* work around the memory leaks */
    shared_ptr<Bucket> new_bucket = (new Bucket(i,j))->self;

    for (auto k = 0; k<8; k++){ 
        /* assign boundary buckets */
        new_bucket->getIndex(k,i,j);
        if (i==N || j==N || i<0 || j<0)
            new_bucket->neighbours[k] = bb;
    }

    for (auto k=0; k<8; k++) { /* go counter clockwise until nullptr */

        /* set links to each other */
        current->neighbours[cd] = new_bucket; 
        new_bucket->neighbours[inc(cd, 4)] = current->self;

        /* go in previous even dir */
        if(inc(cd,7)%2) next_dir = inc(cd, 6);
        else next_dir = inc(cd, 7);
        
        if (current->neighbours[next_dir] == nullptr || current->neighbours[next_dir]->isBnd()) {
            if (inc(cd,7)%2) { /* if previous dir is uneven */
                next_dir = inc(cd, 7);
                if (current->neighbours[next_dir] == nullptr || current->neighbours[next_dir]->isBnd())
                    break;
                /* skip one iteration */
                k++;
                cd = inc(cd,1); 
            } else break;
        } 
        
        current = current->neighbours[next_dir];
        cd = inc(cd,1);
    }
    cd = dir; 
    current = self;
    for (int k = 1; k<8; k++){ /* go clockwise until nullptr */
        /* go in next even dir */
        if(inc(cd,1)%2) next_dir = inc(cd, 2);
        else next_dir = inc(cd, 1);

        /* check if neighbour next_dir exists */
        if (current->neighbours[next_dir] == nullptr || current->neighbours[next_dir]->isBnd()) {
            if (inc(cd,1)%2) {
                next_dir = inc(cd,1);
                if (current->neighbours[next_dir] == nullptr || current->neighbours[next_dir]->isBnd())
                    break;
                /* skip one iteration */
                k++;
                cd = inc(cd,7); 
            } else break;
        }
        current = current->neighbours[next_dir];

        /* update dir of new bucket */
        cd = inc(cd,7);

        /* set links to each other */
        current->neighbours[cd] = new_bucket; 
        new_bucket->neighbours[inc(cd, 4)] = current->self;
    }


}

shared_ptr<Bucket> Bucket::operator() (int i, int j) const{
    auto x = i-ind_i;
    auto y = j-ind_j;
    vector<int> dir(8,0);

    // cout << "x: " << x << " y: " << y << endl;

    /* write as directions */
    if (x>=0 && y>=0) {
        dir[1] = min(x,y);
        dir[0] = x-dir[1];
        dir[2] = y-dir[1];
    }
    if (x<=0 && y>=0) {
        dir[3] = min(-x,y);
        dir[4] = -(x+dir[3]);
        dir[2] = y-dir[3];
    }
    if (x<=0 && y<=0) {
        dir[5] = min(-x,-y);
        dir[4] = -(x+dir[5]);
        dir[6] = -(y+dir[5]);
    }
    if (x>=0 && y<=0) {
        dir[7] = min(x,-y);
        dir[0] = x-dir[7];
        dir[6] = -(y+dir[7]);
    }


    /* go to bucket of index and add if needed */
    shared_ptr<Bucket> current = self;
    shared_ptr<Bucket> next;

    for(int k=0; k<8; k++) {
        while (dir[k]) {
            next = current->neighbours[k];
            if (next == nullptr) { 
                if (self == root) {
                    current->addBucket(k);
                    next = current->neighbours[k];
                } else {
                    return (*root)(i,j);
                }
            }
            if (next->isBnd())
                throw runtime_error("index out of bound in operator()");
            current = next;
            dir[k]--;
        }
    }
    return current;
}

void Bucket::addPoint(double x, double y) {
    Point pnt(x,y);
    auto pos = find_if(points.begin(), points.end(), [pnt](auto s) {
        return s < pnt;
    });
    points.insert(pos, move(pnt));
}

int Bucket::fillCoordinates(int stat){

    if (!(coordinates.empty())) return 0;

    if (r() != root->r()) {
        if (stat == 0) {

            /* get data from right processor */
            int no_bucket = i()*N+j();
            MPI_Send(&no_bucket,1,MPI_INT,r(),TAG_NOTIFY,MPI_COMM_WORLD);
            return 1;
        } 
        else if (stat == 1) {

            /* receive and save points*/
            MPI_Recv(buffer,MAX_PNTS,MPI_DOUBLE,r(),TAG_DATA,MPI_COMM_WORLD,MPI_STATUS_IGNORE);

            coordinates.push_back(buffer[0]);

            for (auto k=0; k<2*buffer[0]; k+=2) {
                coordinates.push_back(buffer[k+1]);
                coordinates.push_back(buffer[k+2]);
                addPoint(buffer[k+1],buffer[k+2]);
            }
            
            return 0;
        } else throw runtime_error("invalid status in fillCoordinates");
    }

    /* number of points in this bucket 
       pfusch distribution  */
    int cnt = rand() % (MAX_PNTS+1);
    for (auto k=0; k<10; k++)
        cnt = max(min(cnt, rand() % (MAX_PNTS+1)),1);

    total_points += cnt;
    coordinates.push_back(cnt); // set amount of points 
    
    double xmin = i()/static_cast<double>(N);
    double xmax = (i()+1)/static_cast<double>(N);
    double ymin = j()/static_cast<double>(N);
    double ymax = (j()+1)/static_cast<double>(N);

    /* assign random coordinates in right bucket */
    for (auto k=0; k<cnt; k++){
        double x = xmin + static_cast<double>(rand())/( static_cast<double>(RAND_MAX/(xmax-xmin)));
        double y = ymin + static_cast<double>(rand())/( static_cast<double>(RAND_MAX/(ymax-ymin)));
        addPoint(x,y);
        coordinates.push_back(x);
        coordinates.push_back(y);
    }

    return 0; 
    // cout << "filled coordinates of (" << i()<<", "<<j()<<
    // ") on processor "<<r()<<endl;

}

/* send points in bucket with number no_bucket to processor destination */
void Bucket::sendCoordinates(int destination, int no_bucket) {

    if (root->r() != r()) throw("called from wrong processor");

    auto ii = no_bucket/N;
    auto jj = no_bucket%N; 

    /* if the desired bucket is not the current bucket call function from right bucket */
    if (i() != ii || j() != jj ) {
        // cout << root->r() << ": wrong bucket" << endl;
        (*root)(ii,jj)->sendCoordinates(destination, no_bucket);
        return;
    }
    /* gerenate data if not available */
    if (coordinates.empty())
        fillCoordinates(0); 

    int tmp = -1;
    MPI_Send(&tmp,1,MPI_INT,destination,TAG_NOTIFY,MPI_COMM_WORLD);
    MPI_Send(coordinates.data(), MAX_PNTS, MPI_DOUBLE, destination, TAG_DATA, MPI_COMM_WORLD);
}

/* get points of current bucket
   return 0 if finished 
   return 1 if waiting for data to be sent

   points are written in pnts
   the input stat indicates the status of the program
*/
int Bucket::getPoints(vector<Point>& pnts, int stat) {
    /* get or gerenate data if not available, 
    check if coordinates are empty because points can
    remain empty if no point in bucket */
    int ret_val = 0;
    if (coordinates.empty()) {
        ret_val = fillCoordinates(stat);
        if (ret_val==0) pnts = points;
    } else pnts = points;
    return ret_val;
}

/* source for how to provide fairness:
https://www.mcs.anl.gov/research/projects/mpi/tutorial/gropp/node93.html#Node93
*/
void Bucket::doSomething() {

    if (self != root) throw("doSomething can only be called from root element");

    MPI_Request requests[MAX_PROC]; 
    MPI_Status  statuses[MAX_PROC]; 
    int         indices[MAX_PROC]; 
    int         buf[MAX_PROC]; 
    auto size = P*P;
    int ndone;
    bool open_request = false;
    for (auto i=0; i<size; i++)  
        MPI_Irecv( buf+i, 1, MPI_INT, i, TAG_NOTIFY, MPI_COMM_WORLD, &requests[i] ); 

    bool startup = false;
    bool init = true;
    int finished = 0;

    vector<Point> pnts;
    int cnt = 0; // to iterate through pnts
    int step = 0;
    vector<int> buf_status;
    vector<int> buf_buf;

    int I = N/P;
    int ii, jj; /* global indec of current bucket */
    shared_ptr<Bucket> current; /* pointer to current bucket */

    /* do computations until communication with other processors is required 
    ( meaning some function returns 1) or everything is done */

    /* iterate through buckets belonging to processor with rank root->r()*/
    for (ii = root->r()%P*I; ii<(root->r()%P+1)*I; ii++) { 
        for (jj = root->r()/P*I; jj<(root->r()/P+1)*I; jj++) {
            current = (*root)(ii, jj);
            if (current->getPoints(pnts, 0)) 
                throw runtime_error("initialize for "+to_string(ii)+","+to_string(jj)+" called from wrong processor "+to_string(current->r()));
            cnt = 0;

            while (!startup && cnt < pnts.size()) {
                if (init) {
                    if (step == 0)
                        current->poly = move(Polygon(pnts[cnt]));
                    step = current->initialize(step);
                    if (step == 0) { /* init has finished */
                        init = false;
                    } else {
                        startup = true;
                        break;
                    }
                } if (!init) { /* this is not an else!*/
                    step = current->calculateDelauney(step);
                    if (step == 0) { /* delauney has finished */
                        cnt++;
                        init = true;
                    } else {
                        startup = true;
                        break;
                    }
                }
            }
            if (startup) break;
        }
        if (startup) break;
    }

    bool cond = true;

    if (!startup) {
        if (P==1) {
            cond = false;
        }
        if (root->r() == 0) finished++;
        else {
            int tmp = -42;
            MPI_Send(&tmp,1,MPI_INT,0,TAG_NOTIFY,MPI_COMM_WORLD);
        }
    }

    while(cond) { 

        MPI_Waitsome( size, requests, &ndone, indices, statuses ); 
        for (auto i=0; i<ndone; i++) { 
            auto j = indices[i]; 
            // printf( "on %d msg from %d with value (%d,%d)\n",  
            //         r(),
            //         statuses[i].MPI_SOURCE,  
            //         buf[j]/N,
            //         buf[j]%N); 

            if (buf[j]>-1) 
                sendCoordinates(statuses[i].MPI_SOURCE, buf[j]);
            else if (buf[j] == -1 || startup) {
                startup = false;
                start_again:
                // cout << root->r() << ": cnt = " << cnt << " step = " << step << " init = " << init << endl;

                if (cnt < pnts.size()) {
                    if (init) {
                        if (step == 0) /* call for new point */
                            current->poly = move(Polygon(pnts[cnt]));
                        step = current->initialize(step);
                        if (step == 0) { /* init has finished*/
                            init = false;
                        } 
                    } 
                    if (!init) { /* this is different than else! */
                        step = current->calculateDelauney(step);
                        if (step == 0) { /* delauney has finished */
                            cnt++;
                            init = true;
                            goto start_again; 
                        } 
                    }
                } else {
                    
                    /* calculation finished*/ 
                    if (ii == (root->r()%P+1)*I-1 && jj==(root->r()/P+1)*I-1) { 
                        if (root->r() == 0) finished++;
                        else {
                            int tmp = -42;
                            MPI_Send(&tmp,1,MPI_INT,0,TAG_NOTIFY,MPI_COMM_WORLD);
                        }
                        cout <<  "------ PROCESSOR " << root->r() <<" FINISHED ------" << endl;
                        
                    } 
                    else { /* build a for loop like increase */
                        if (ii < (root->r()%P+1)*I-1) ii++;
                        else {
                            ii = root->r()%P*I;
                            jj++;
                        }
                        current = (*root)(ii, jj);
                        cnt = 0;
                        if (current->getPoints(pnts, 0)) /* communication required */
                            throw runtime_error("initialize for "+to_string(ii)+","+to_string(jj)+" called from wrong processor "+to_string(current->r()));
                        goto start_again;
                    }
                }
            }
            else if (buf[j] == -42 && r()==0) {
                finished++;
                if (finished == P*P) {
                    int tmp = -43;
                    MPI_Send(&tmp,1,MPI_INT,1,TAG_NOTIFY,MPI_COMM_WORLD);
                    cond = false;
                    break;
                }
            } else if (buf[j] == -43) {
                if (r() == P*P-1) {
                    cond = false;
                    break;
                }
                if (r() < P*P-1) {
                    int tmp = -43;
                    MPI_Send(&tmp,1,MPI_INT,r()+1,TAG_NOTIFY,MPI_COMM_WORLD);
                    cond = false;
                    break;
                }
            }
            MPI_Irecv( buf+j, 1, MPI_INT, j, 
                TAG_NOTIFY, MPI_COMM_WORLD, &requests[j]); 
        } 
    } 
    cout << "in total " << total_points << " points on processor " << r() << endl;
}

/* return values:
   -------------- 
   0: completed
   1: waiting for points to be sent
    
*/
int Bucket::initialize(int step) {

    /* current (i,j) */
    auto ii = i();
    auto jj = j();

    /* build polygon for first time */ 
    vector<Point> tmp;
    auto add = [this](const Point& p) {poly.addPoint(p);};
    
    if (step == 1)
        goto resume;

    if (step != 1 && step !=0) 
        throw runtime_error("invalid step in initialize");

    for (init_dir_i=-1; init_dir_i<2; init_dir_i+=2 ) {
        for (init_dir_j=-1; init_dir_j<2; init_dir_j+=2 ) {
            init_incr = 1;
            while (true){                

                /* add ghost point if out of bounds */
                if (indexOutOfBnds(ii+init_incr*init_dir_i,jj+init_incr*init_dir_j)) {
                    add(Point(max(init_dir_i, 0), max(init_dir_j,0)));
                    break;
                }
                else if ((*self)(ii+init_incr*init_dir_i,jj+init_incr*init_dir_j)->getPoints(tmp, 0)) {
                    return 1;
                    resume:
                    (*self)(ii+init_incr*init_dir_i,jj+init_incr*init_dir_j)->getPoints(tmp, 1);
                }
                
                /* add points (see sketch) and go in diagonal direction if cell is empty*/
                if (!tmp.empty()) {
                    for_each(tmp.begin(), tmp.end(), add);
                    tmp.clear();
                    break;
                }
                else {
                    cout << root->r() << ": init_incr ++" << endl;
                    init_incr++;
                }
            }
        }
    }

    /* make polygon convex */
    bool cond = false;
    int k = 0;
    if (poly.points.size()>3) cond = true; 
    while (cond) {
        Point v = poly.points[(k+1)%poly.points.size()]-poly.points[k];
        Point w = poly.points[(k+2)%poly.points.size()]-poly.points[k];

        /* convex if cross product positiv */ 
        if (v.x*w.y-v.y*w.x < 0) {
            poly.points.erase(poly.points.begin()+(k+1)%poly.points.size());
            if (poly.points.size() == 3) cond = false;
        } else {
            k = (k+1)%poly.points.size();
            if (k==0) cond = false;
        }
    }

    /* calculate voronoi points */
    poly.calculateVoronoi();
    // cout << "****** init finished for  Bucket (" <<i() << ", " << j() << ") "  << endl;
    return 0;
}

int Bucket::calculateDelauney(int step){ 

    vector<Point> candidates;

    if (step == 0) {
        it = 0;
        break_calculation = 0;
    }
    while (it <poly.points.size() && break_calculation < 20) {
        break_calculation++;

        auto rad = poly.radii[it];
        auto pnt = poly.voronoi[it];
        /* distance of buckets that can contain candidates */
        /* THIS IS NOT CORRECT actually it should be
            int n = min(static_cast<int>(rad*N+1),N); 
            but that was extremely slow due to some other error choosing the points
            I know it yields a wrong output...
         */
        int n = min(static_cast<int>(rad*N+1),10);
        /* indices of bucket containing pnt */
        int pi = max(0,static_cast<int>(pnt.x*N));
        int pj = min(static_cast<int>(pnt.y*N), N-1); 

        /* resume calculation at resume: */
        if (step==1)  goto resume;
        
        /* calculate Delauney neighbour candidates for pnt */
        poly.V.clear();
        for (di=-n; di<n+1; di++) {
            for (dj=-n; dj<n+1; dj++) {

                if (indexOutOfBnds(pi+di, pj+dj)) continue;
                if((*self)(pi+di, pj+dj)->getPoints(candidates, 0)) {
                    return 1;
                    resume: 
                    (*self)(pi+di, pj+dj)->getPoints(candidates, 1);
                } 
                
                for (auto p : candidates) {
                    /* add if in right range and not already in points or center*/
                    if (Point::dist(p,pnt)-1e-8 < rad 
                    && find(poly.points.begin(), poly.points.end(), p) == poly.points.end()
                    && p!=poly.c){
                        poly.V.push_back(p);
                        /* break if one point is found */
                        di = n+1;
                        dj = n+1;
                        break;
                    }
                }
                candidates.clear();
                
            }
        }

        if (false) {
            test:
            break_calculation++;
        }

        if (poly.V.empty()) {
            it++;
        } 
        else {

            Point v = poly.V.back();
            poly.V.pop_back();
            Point a = (poly.c+v)/2;
            Point b = a + Point((v-a).y, (a-v).x);

            /* returns true if p in Hv */
            auto inHv = [&a, &b, this](Point& p) {
                double ths = 1e-16;
                /*  check if poly.c and p are in the same half plane defined by a and b */
                if (a.x == b.x) { 
                    if (poly.c.x-ths <= a.x && p.x-ths <= a.x) {
                        return true;
                    }
                    else if (poly.c.x >= a.x-ths && p.x >= a.x-ths) {
                        return true;
                    }
                } else {
                    /* bisector as linear function */
                    auto h = [&a, &b](double x) {
                        return (a.y-b.y)/(a.x-b.x)*(x-b.x)+b.y;
                    };
                    /* poly.c in subgraph of h*/
                    if (poly.c.y-ths <= h(poly.c.x) && p.y-ths <= h(p.x)) {
                        return true;
                    }
                    /* poly.c in supergraph of h*/
                    else if (poly.c.y >= h(poly.c.x)-ths && p.y >= h(p.x)-ths) {
                        return true;
                    }
                }  
                return false;
            };     

            int old_size = poly.voronoi.size();
            auto ind = [old_size](int k) {
                k = k%old_size;
                if (k >= 0) return k;
                return old_size+k;
            };

            /* find first and last voronoi point in half plane */
            int first = -1;
            int last = -1;
            for (int k = 0; k<old_size; k++) {
                if (inHv(poly.voronoi[k]) && 
                    !inHv(poly.voronoi[ind(k-1)])) first = k;
                if (!inHv(poly.voronoi[ind(k+1)]) && 
                    inHv(poly.voronoi[k])) last = k;   
            }
            

            // cout << endl << root->r()  << "." << it 
            // << " of "<< poly.points.size() << endl 
            // << "v = " << v << endl 
            // << "a = " << a << endl
            // << "b = " << b << endl
            // << "center = " << poly.c << endl
            // << poly << endl;
            // cout << root->r() << "." << it << ": last = " << last << " first = " << first << endl; 

            if (first==-1 || last ==-1) {
                
                /* this is not the correct way to deal with the problem. 
                It actually should never occure. However it did in various occations. WHY??
                I think that is the main problem leading to the solution of the other bugs.*/ 
                    
                it++;
                step = 2;
                continue;
            }

            Point o1, o2;
            if(!circumcenter(o1, static_cast<Point>(poly.points[ind(last+1)]), v, static_cast<Point>(poly.c))) {
                cout << root->r() << ": circumcenter problem between " <<
                     poly.points[ind(last+1)] << v << static_cast<Point>(poly.c) << endl;
                // throw runtime_error("circumcenter problem");
                if (Point::dist(v,poly.c) < Point::dist(poly.points[ind(last+1)], poly.c)) {
                    poly.points.erase(poly.points.begin()+ind(last+1));
                    poly.addPoint(v);
                    poly.calculateVoronoi();
                } else { 
                    /* this is not the correct way to deal with the problem 
                    there could be another candidate in that needs to be considered too */ 
                    it++;
                }
                step = 2;
                continue;
            }

            if (!circumcenter(o2, static_cast<Point>(poly.points[first]), v, static_cast<Point>(poly.c)) ){
                cout << root->r() << ": circumcenter problem between " <<
                     poly.points[first] << v << static_cast<Point>(poly.c) << endl;
                // throw runtime_error("circumcenter problem");

                if (Point::dist(v,poly.c) < Point::dist(poly.points[first], poly.c)) {
                    poly.points.erase(poly.points.begin()+first);
                    poly.addPoint(v);
                    poly.calculateVoronoi();
                } else { 
                    /* this is not the correct way to deal with the problem 
                    there could be another candidate in that needs to be considered too */ 
                    it++;
                }
                step = 2;
                continue;
            }
            /* delete points */
            if (ind(last+1) == ind(first-1)) { /* only one voronoi point outside */
                poly.voronoi.erase(poly.voronoi.begin()+ind(last+1));
                poly.radii.erase(poly.radii.begin()+ind(last+1));

            }  
            else { /* more than one point outside */
                
                if (ind(last+1) < first) {
                    poly.voronoi.erase(poly.voronoi.begin()+ind(last+1), 
                        poly.voronoi.begin()+first);

                    poly.radii.erase(poly.radii.begin()+ind(last+1), 
                        poly.radii.begin()+first);

                }
                else {
                    poly.voronoi.erase(poly.voronoi.begin()+ind(last+1), 
                        poly.voronoi.end());

                    poly.radii.erase(poly.radii.begin()+ind(last+1), 
                        poly.radii.end());

                    poly.voronoi.erase(poly.voronoi.begin(), 
                        poly.voronoi.begin()+first);

                    poly.radii.erase(poly.radii.begin(), 
                        poly.radii.begin()+first);

                }


                if (ind(last+2) < first) {
                    poly.points.erase(poly.points.begin()+ind(last+2), 
                        poly.points.begin()+first);
                } else {
                    poly.points.erase(poly.points.begin()+ind(last+2), 
                        poly.points.end());
                    
                    poly.points.erase(poly.points.begin(), 
                        poly.points.begin()+first);
                }
            }

            /* add new values */ 
            poly.addPoint(v);
            auto tmp = find(poly.points.begin(), poly.points.end(), v);
            int index = distance(poly.points.begin(), tmp);

            /* v is first entry */
            if (index == 0) { 

                poly.voronoi.push_back(o1);
                poly.voronoi.insert(poly.voronoi.begin(), o2);
                poly.radii.push_back(Point::dist(v, o1));
                poly.radii.insert(poly.radii.begin(), Point::dist(v, o2));

                it = 0;
            } else {

                poly.voronoi.insert(poly.voronoi.begin()+index-1,o2);
                poly.radii.insert(poly.radii.begin()+index-1,Point::dist(v, o2));
                poly.voronoi.insert(poly.voronoi.begin()+index-1,o1);
                poly.radii.insert(poly.radii.begin()+index-1,Point::dist(v, o1));

                it = index-1;
            }
        }
        step = 2;
    }
    // cout << "****** calculation finished for point " << poly.c << " in Bucket (" <<i() << ", " << j() << ") " << endl;

    if (MAX_PROC < 1000 && MAX_PNTS <10 && N<20) {
        auto tmp = find(points.begin(), points.end(), static_cast<Point>(poly.c));
        int index = distance(points.begin(), tmp);
        poly.printPoints(to_string(i()*1000+j()*10+index));
    }
    return 0;
}

/********* POLYGON *********/

/* returns false if circumcenter cannot be calculated */
template <typename T> 
bool circumcenter(T& ret, const T& a, const T& b, const T& c) {
    auto d = 2 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
    if (d == 0)
        return false; 
        // throw runtime_error("lines are parallel in circumcenter");
    ret.x = ((a.x * a.x + a.y * a.y) * (b.y - c.y) + (b.x * b.x + b.y * b.y) * (c.y - a.y) + (c.x * c.x + c.y * c.y) * (a.y - b.y)) / d;
    ret.y = ((a.x * a.x + a.y * a.y) * (c.x - b.x) + (b.x * b.x + b.y * b.y) * (a.x - c.x) + (c.x * c.x + c.y * c.y) * (b.x - a.x)) / d;
    return true;
}
template bool circumcenter<Point>(Point& ret, const Point& a, const Point& b, const Point& c);

// add points in counter clockwise order
void Polygon::addPoint(double p0, double p1) {
    addPoint(PointPoly(this, p0, p1));
}

void Polygon::addPoint(const Point& pin) {
    PointPoly p(this, pin);

    /* order points according to overloaded operators in PolyPoint*/
    bool exists = false;
    auto pos = std::find_if(points.begin(), points.end(), [p,&exists](auto s) {
        if (s==p) {
            exists=true;
        }
        return s < p;
    });
    if (exists) {
        // cout << ": point already exists" << endl;
        return;
    } /* if element alread exists don't add */
    points.insert(pos, move(p));
}

/* calculate voronoi points and radii */
void Polygon::calculateVoronoi(void) {

    voronoi.clear();
    radii.clear();
    if (points.size() < 2) return;
    PointPoly cc(this); // to be circumcenter

    for(auto it = points.begin(); it!=points.end()-1; it++){
        auto next = it+1;
        circumcenter(cc, *it, *next, c);
        voronoi.push_back(cc); // cc is copied
        radii.push_back(Point::dist(cc,c));
    }
    circumcenter(cc, points.front(), points.back(), c);
    voronoi.push_back(cc);
    radii.push_back(Point::dist(cc,c));
}

std::ostream &operator<<(std::ostream &os, const Polygon &poly) {
    os << "points = [ ";
    for (const auto& p: poly.points)
        os << p << ", ";
    os << "]" << endl << "voronoi = [ " ;
    for (const auto& p: poly.voronoi)
        os << p << ", ";
    os << "]" << endl << "radii = [ " ;
    for (const auto& p: poly.radii)
        os << p << ", ";
    os << "]";
    return os;
}

void Polygon::printPoints(string no) {

    ofstream myfile;
    string path = "outdata/";

    
    myfile.open(path+"polyPoints"+no+".txt");
    if (myfile.is_open()) {
        myfile << "[" << c << ",";

        for (const auto& p: points)
                myfile << p << ", ";
        myfile << "]" << endl;
        myfile.close();
    }
    // else cout << "Unable to open file";

    myfile.open(path+"voronoiPoints"+no+".txt");
    if (myfile.is_open()) {
        myfile << "[";
        for (const auto& p: voronoi)
                myfile << p << ", ";
        myfile << "]" << endl;
        myfile.close();
    }
    else cout << "Unable to open file";
}

/* order is counter clock wise*/
bool Polygon::PointPoly::operator> (const Polygon::PointPoly& other) const {
    PointPoly a = *this-poly->c;
    PointPoly b = other-poly->c;

    /* to have a beginning (at angle == 0 in unit circle) */
    if (a.y>=0 && b.y<0) return true;
    if (a.y<0 && b.y>=0) return false;
    if (a.y==0 && b.y==0) {
        if (a.x>0 || b.x>0) return a.x<b.x;
        else return a.x>b.x;
    }

    /* check order  */
    auto cross_prod = a.x*b.y-a.y*b.x;
    if (cross_prod > 0) return true;
    if (cross_prod < 0) return false;

    /* if points have same angle take nearer one first */
    if (a.x*a.x+a.y*a.y < b.x*b.x+b.y*b.y) return 1;
    
    return false;
}

bool Polygon::PointPoly::operator>= (const Polygon::PointPoly& other) const {
    if (*this>other || *this==other) return true;
    return false;
}

bool Polygon::PointPoly::operator< (const Polygon::PointPoly& other) const {
    if (!(*this>=other)) return true;
    return false;
}

bool Polygon::PointPoly::operator<= (const Polygon::PointPoly& other) const {
    if (!(*this>other)) return true;
    return false;
}
