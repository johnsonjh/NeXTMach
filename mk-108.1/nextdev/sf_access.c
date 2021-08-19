/*	@(#)sf_access.c 	1.0	02/07/90	(c) 1990 NeXT	*/

/* 
 **********************************************************************
 * sf_access.c -- Floppy/SCSI access arbitration mechanism
 *
 * HISTORY
 * 07-Feb-89	Doug Mitchell at NeXT
 *	Created.
 *
 **********************************************************************
 */ 

#import <sys/types.h>
#import <kern/queue.h>
#import <kern/xpr.h>
#import <next/spl.h>
#import <nextdev/sf_access.h>

int sfa_arbitrate(sf_access_head_t sfahp, sf_access_device_t sfadp)
{
	int s;
	
	/*
	 * called to gain access to scsi/floppy hardware. If hardware not 
	 * available, we register our intent to use it and return.
	 */
	
	XPR_SFA(sfadp, ("sfa_arbitrate: "));
	if(sfadp->sfad_flags & SFDF_OWNER) {
		/*
		 * this driver already is an owner.
		 */
		if(sfadp->sfad_flags & SFDF_EXCL) {
			if(!(sfahp->sfah_flags & SFHF_EXCL))
				panic("sfa_arbitrate: onwer changed priority");
		}
		XPR_SFA(sfadp, ("CURRENT OWNER\n"));
		(*sfadp->sfad_start)(sfadp->sfad_arg);
		return(0);
	}
	
	s = spldma();
	simple_lock(&sfahp->sfah_lock);
	if(sfahp->sfah_busy) {
		if(!(sfadp->sfad_flags & SFDF_EXCL) &&
		   !(sfahp->sfah_flags & SFHF_EXCL) &&
		    (sfahp->sfah_excl_q == 0)) {
		 	/*
			 * Well, the bus isn't idle, but neither this device,
			 * the current owner, or any enqueued device needs 
			 * exclusive access, so we'll grant access.
			 */
			goto gotit;  
		}
		   
		/*
		 * can't do I/O now. Register our intent.
		 */
		queue_enter(&sfahp->sfah_q, 
			    sfadp, 
			    sf_access_device_t,
			    sfad_link);
		sfahp->sfah_wait_cnt++;
		if(sfadp->sfad_flags & SFDF_EXCL)
			sfahp->sfah_excl_q++;
		simple_unlock(&sfahp->sfah_lock);
		XPR_SFA(sfadp, ("BLOCKING\n"));
		splx(s);
		return(1);
	}
	else {
gotit:
		/* 
		 * hardware available. Go for it.
		 */
		sfahp->sfah_busy++;
		if(sfadp->sfad_flags & SFDF_EXCL)
			sfahp->sfah_flags |= SFHF_EXCL;
		simple_unlock(&sfahp->sfah_lock);
		XPR_SFA(sfadp, ("H/W AVAILABLE\n"));
		sfadp->sfad_flags |= SFDF_OWNER;
		splx(s);
		(*sfadp->sfad_start)(sfadp->sfad_arg);
		return(0);
	}
} /* sfa_arbitrate() */

void sfa_relinquish(sf_access_head_t sfahp, 
	sf_access_device_t sfadp,
	int last_device)		/* SF_LD_SCSI / SF_LD_FD */
{
	/*
	 * Called when a driver is thru with the hardware. Give it to next
	 * driver if necessary.
	 */
	
	sf_access_device_t next_sfadp;
	int s;
	
	XPR_SFA(sfadp, ("sfa_relinquish: "));
	if(!(sfadp->sfad_flags & SFDF_OWNER)) {
		/* 
		 * not an error; just kind of weird...
		 */
#ifdef	DEBUG	
		printf("sfa_relinquish: NOT OWNER; sfadp = 0x%x\n", sfadp);
#endif	DEBUG
		XPR_SFA(sfadp, ("NOT OWNER\n"));
		return;
	}
	sfadp->sfad_flags &= ~SFDF_OWNER;
	s = spldma();
	simple_lock(&sfahp->sfah_lock);
	
	/*
	 * Update state of queue head to reflect this relinquish.
	 */
	if(last_device != SF_LD_NONE)
	 	sfahp->sfah_last_dev = last_device;
	sfahp->sfah_busy--;
	if(sfadp->sfad_flags & SFDF_EXCL) 
		sfahp->sfah_flags &= ~SFHF_EXCL;
	if(sfahp->sfah_wait_cnt) {
		
		/*
		 * Another device is waiting. Grant access to every device in
		 * queue until we get to a device which requires exclusive
		 * access. All devices after that one have to wait. (So does
		 * the device needing exclusive access, if we granted access
		 * to anyone before it, or any other devices are currently 
		 * active...)
		 *
		 * Note we're at spldma() on entering the loop.
		 */
		ASSERT(!queue_empty(&sfahp->sfah_q));
		while(!queue_empty(&sfahp->sfah_q)) {
			next_sfadp = 
			     (sf_access_device_t)queue_first(&sfahp->sfah_q);
			if(next_sfadp->sfad_flags & SFDF_EXCL) {
				if(sfahp->sfah_busy) {
				    /* too late */
				    XPR_SFA(sfadp, ("QUEUED EXCL MISSED\n"));
				    simple_unlock(&sfahp->sfah_lock);
				    splx(s);
				    break;		
				}
			}
			/*
			 * OK, this device can go.
			 */
			sfahp->sfah_wait_cnt--;
			sfahp->sfah_busy++;
			if(next_sfadp->sfad_flags & SFDF_EXCL) {
				sfahp->sfah_excl_q--;
				sfahp->sfah_flags |= SFHF_EXCL;
			}
			queue_remove(&sfahp->sfah_q,
					next_sfadp,
					sf_access_device_t,
					sfad_link);
			simple_unlock(&sfahp->sfah_lock);
			splx(s);
			next_sfadp->sfad_flags |= SFDF_OWNER;
			XPR_SFA(sfadp, ("DOING CALLOUT @ 0x%x\n",
					next_sfadp->sfad_start));
			(*next_sfadp->sfad_start)(next_sfadp->sfad_arg);
			if(next_sfadp->sfad_flags & SFDF_EXCL)
				break;			/* no more */
			if(!queue_empty(&sfahp->sfah_q)) {
				s = spldma();
				simple_lock(&sfahp->sfah_lock);
			}			
		} /* scanning wait queue */
	}
	else {
		splx(s);
		simple_unlock(&sfahp->sfah_lock);
		XPR_SFA(sfadp, ("QUEUE EMPTY\n"));
	}
} /* sfa_relinquish() */

void sfa_abort(sf_access_head_t sfahp, 
	sf_access_device_t sfadp,
	int last_device)
{
	/*
	 * called upon abnormal termination of I/O; device may be owner of
	 * hardware or may be enqueued to get access to hardware.
	 */
	
	int s;
	
	XPR_SFA(sfadp, ("sfa_abort: "));
	if(sfadp->sfad_flags & SFDF_OWNER) {
		XPR_SFA(sfadp, ("OWNER\n"));
		sfa_relinquish(sfahp, sfadp, last_device);
	}
	else {
		/* 
		 * search thru queue, looking for this device. If found,
		 * remove it.
		 */
		sf_access_device_t queued_sfadp;
		
		s = spldma();
		simple_lock(&sfahp->sfah_lock);
		queued_sfadp = (sf_access_device_t)queue_first(&sfahp->sfah_q);
		while(!queue_end(&sfahp->sfah_q, (queue_t)queued_sfadp)) {
			if(queued_sfadp == sfadp) {
				queue_remove(&sfahp->sfah_q,
			     		queued_sfadp,
			     		sf_access_device_t,
			     		sfad_link);
				sfahp->sfah_wait_cnt--;
				if(sfadp->sfad_flags & SFDF_EXCL)
					sfahp->sfah_excl_q--;
				XPR_SFA(sfadp, ("REQUEST DEQUEUED\n"));
				break;
			}
			/*
			 * try next element in queue 
			 */
			queued_sfadp =
			      (sf_access_device_t)queued_sfadp->sfad_link.next;
		}
		splx(s);
		simple_unlock(&sfahp->sfah_lock);
	}
} /* sfa_abort() */








