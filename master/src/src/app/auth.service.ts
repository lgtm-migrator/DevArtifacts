import { Injectable } from '@angular/core';

import { Observable } from 'rxjs/Observable';
import 'rxjs/add/observable/of';
import 'rxjs/add/operator/do';
import 'rxjs/add/operator/delay';

@Injectable()
export class AuthService {
	private isLoggedIn: boolean = false;

	// store the URL so we can redirect after logging in
	private redirectUrl: string;

	public login(): Observable<boolean> {
		return Observable.of(true).delay(1000).do(val => this.isLoggedIn = true);
	}

	public logout(): void {
		this.isLoggedIn = false;
	}
}