///*
// * Copyright 2012-2015 the original author or authors.
// *
// * Licensed under the Apache License, Version 2.0 (the "License");
// * you may not use this file except in compliance with the License.
// * You may obtain a copy of the License at
// *
// *      http://www.apache.org/licenses/LICENSE-2.0
// *
// * Unless required by applicable law or agreed to in writing, software
// * distributed under the License is distributed on an "AS IS" BASIS,
// * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// * See the License for the specific language governing permissions and
// * limitations under the License.
// */
//
//package com.ubs.network.api.rest.services.jenkins;
//
//import static org.assertj.core.api.Assertions.assertThat;
//
//import org.junit.Test;
//import org.junit.runner.RunWith;
//import org.springframework.beans.factory.annotation.Autowired;
//import org.springframework.boot.test.context.SpringBootTest;
//import org.springframework.test.context.junit4.SpringRunner;
//import org.springframework.web.client.RestTemplate;
//
//@RunWith(SpringRunner.class)
////@DataJpaTest
//@SpringBootTest(webEnvironment = WebEnvironment.RANDOM_PORT, classes = { com.ubs.network.api.rest.services.jenkins.loader.AppLoader })
//public class BaseTest {
//
//	@After
//	public void init(){
//	}
//
//	@Before
//	public void destroy(){
//	}
//
//	@Autowired
//	private RestTemplate restTemplate;
//
//	@Test
//	public void contextLoads() {
//		assertThat(restTemplate).isNotNull();
//	}
//
//}

//Poll newPoll = new Poll();
//
//		Set<Options> options = new HashSet<>();
//		newPoll.setOptions(options);
//
//		Option option1 = new Option();
//		option1.setValue("Red");
//		options.add(option1);
//
//		Option option2 = new Option();
//		option2.setValue("Blue");
//		options.add(option2);
//URI pollLocation = client.createPoll(newPoll);