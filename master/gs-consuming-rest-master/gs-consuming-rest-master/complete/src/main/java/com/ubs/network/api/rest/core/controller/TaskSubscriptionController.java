package com.ubs.network.api.rest.core.controller;

import javax.inject.Inject;

import com.ubs.network.api.rest.core.controller.interfaces.ITaskSubscriptionController;
import com.ubs.network.api.rest.core.model.dto.TaskSubscriptionDTO;
import com.ubs.network.api.rest.core.model.entities.TaskSubscriptionEntity;
import org.springframework.data.domain.Pageable;
import org.springframework.http.HttpHeaders;
import org.springframework.http.HttpStatus;
import org.springframework.http.ResponseEntity;
import org.springframework.web.bind.annotation.PathVariable;
import org.springframework.web.bind.annotation.RequestBody;
import org.springframework.web.bind.annotation.RequestMapping;
import org.springframework.web.bind.annotation.RequestMethod;
import org.springframework.web.bind.annotation.RestController;
import org.springframework.web.servlet.support.ServletUriComponentsBuilder;

import com.apress.domain.Vote;
import com.apress.repository.VoteRepository;
import com.wordnik.swagger.annotations.Api;
import com.wordnik.swagger.annotations.ApiOperation;
import com.wordnik.swagger.annotations.ApiResponse;
import com.wordnik.swagger.annotations.ApiResponses;

@RestController("subscriptionController")
@RequestMapping("/v3/")
@Api(value = "votes", description = "Votes API")
public class TaskSubscriptionController<E extends TaskSubscriptionEntity, D extends TaskSubscriptionDTO> extends CoreBaseController<E, D> implements ITaskSubscriptionController<E, D> {

    @Inject
    private TaskSubscriptionRepository voteRepository;

    @RequestMapping(value="/polls/{pollId}/votes", method=RequestMethod.POST)
    @ApiOperation(value = "Casts a new vote for a given poll", notes="The newly created vote Id will be sent in the location response header",
            response = Void.class)
    @ApiResponses(value = {@ApiResponse(code=201, message="Vote Created Successfully", response=Void.class) })
    public ResponseEntity<Void> createVote(@PathVariable Long pollId, @RequestBody Vote vote) {
        vote = voteRepository.save(vote);

        // Set the headers for the newly created resource
        HttpHeaders responseHeaders = new HttpHeaders();
        responseHeaders.setLocation(ServletUriComponentsBuilder.fromCurrentRequest().path("/{id}").buildAndExpand(vote.getId()).toUri());

        return new ResponseEntity<>(null, responseHeaders, HttpStatus.CREATED);
    }

    @RequestMapping(value="/polls/{pollId}/votes", method=RequestMethod.GET)
    @ApiOperation(value = "Retrieves all the votes", response=Vote.class, responseContainer="List")
    public Iterable<Vote> getAllVotes(@PathVariable Long pollId, Pageable pageable) {
        return voteRepository.findByPoll(pollId);
    }
}